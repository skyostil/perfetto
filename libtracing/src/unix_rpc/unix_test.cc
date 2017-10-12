#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <list>
#include <map>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/data_source_descriptor.h"
#include "libtracing/core/producer.h"
#include "libtracing/core/shared_memory.h"
#include "libtracing/core/task_runner_proxy.h"
#include "libtracing/src/core/base.h"
#include "libtracing/unix_rpc/unix_service.h"
#include "libtracing/unix_rpc/unix_service_connection.h"

namespace {

using perfetto::DataSourceConfig;
using perfetto::DataSourceDescriptor;
using perfetto::DataSourceID;
using perfetto::DataSourceInstanceID;
using perfetto::Producer;
using perfetto::Service;
using perfetto::SharedMemory;
using perfetto::TaskRunnerProxy;
using perfetto::UnixService;
using perfetto::UnixServiceConnection;

const char kServiceSocketName[] = "/tmp/perfetto_test_sock";

class PoorManTaskRunner : public TaskRunnerProxy {
 public:
  PoorManTaskRunner() { FD_ZERO(&fd_set_); }

  ~PoorManTaskRunner() override {}

  void Run() {
    for (;;) {
      while (!task_queue_.empty()) {
        std::function<void()> closure = std::move(task_queue_.front());
        task_queue_.pop_front();
        DLOG("[TaskRunner] Running task ...\n");
        closure();
        DLOG("[TaskRunner] ... task done\n");
      }
      DLOG("[TaskRunner] select() num_fds=%lu\n", watched_fds_.size());
      int res = select(FD_SETSIZE, &fd_set_, nullptr, nullptr, nullptr);
      if (res < 0) {
        perror("select failed");
        return;
      }
      if (res == 0) {
        printf("select() returned 0, weird. sleeping.\n");
        usleep(100000);
        continue;
      }
      for (int fd = 0; fd < FD_SETSIZE; ++fd) {
        if (!FD_ISSET(fd, &fd_set_))
          continue;
        DLOG("[TaskRunner] invoking fd callback\n");
        auto fd_and_callback = watched_fds_.find(fd);
        DCHECK(fd_and_callback != watched_fds_.end());
        fd_and_callback->second();
      }
    }
  }

  // TaskRunnerProxy implementation.
  void PostTask(std::function<void()> closure) override {
    task_queue_.emplace_back(std::move(closure));
  }

  void AddFileDescriptorWatch(int fd, std::function<void()> callback) override {
    DCHECK(fd > 0);
    DCHECK(watched_fds_.count(fd) == 0);
    watched_fds_.emplace(fd, std::move(callback));
    FD_SET(fd, &fd_set_);
  }

  void RemoveFileDescriptorWatch(int fd) override {
    DCHECK(fd > 0);
    DCHECK(watched_fds_.count(fd) == 1);
    watched_fds_.erase(fd);
    FD_CLR(fd, &fd_set_);
  }

 private:
  std::list<std::function<void()>> task_queue_;
  std::map<int, std::function<void()>> watched_fds_;
  fd_set fd_set_;
};  // class PoorManTaskRunner.

class TestServiceDelegate : public UnixService::Delegate {
 public:
  TestServiceDelegate(TaskRunnerProxy* task_runner)
      : task_runner_(task_runner) {}

  ~TestServiceDelegate() override {}

  void set_on_data_source_connected_callback(
      std::function<void(DataSourceID)> cb) {
    on_data_source_connected_callback_ = std::move(cb);
  }

  // UnixService::Delegate implementation.
  TaskRunnerProxy* task_runner() const override { return task_runner_; }

  void OnDataSourceConnected(DataSourceID dsid) override {
    if (on_data_source_connected_callback_)
      on_data_source_connected_callback_(dsid);
  }

 private:
  TaskRunnerProxy* task_runner_;
  std::function<void(DataSourceID)> on_data_source_connected_callback_;
};

class TestProducer : public Producer {
 public:
  void CreateDataSourceInstance(const DataSourceConfig& conf,
                                DataSourceInstanceID id) override {
    DLOG("[unix_test.cc] CreateDataSourceInstance name=%s filters=%s "
        "instance_id=%" PRIu64 "\n",
        conf.data_source_name.c_str(), conf.trace_category_filters.c_str(), id);
  }

  void TearDownDataSourceInstance(DataSourceInstanceID id) override {
  }

  void OnConnect() override {
    DLOG("[unix_test.cc] OnConnect() ");
    DCHECK(service);
    SharedMemory* shm = service->GetSharedMemoryForProducer(0);
    DCHECK(shm);
    DLOG("[unix_test.cc] Succesfully wrote to the shared memory\n");
    memcpy(shm->start(), "bazinga", 8);
    service->NotifyPageReleased(0, 1);
  }

  Service* service = nullptr;
};

int ServiceMain() {
  unlink(kServiceSocketName);

  PoorManTaskRunner task_runner;
  TestServiceDelegate delegate(&task_runner);
  std::unique_ptr<UnixService> svc =
      UnixService::CreateInstance(kServiceSocketName, &delegate);
  if (!svc) {
    perror("Could not create service.");
    return 1;
  }
  delegate.set_on_data_source_connected_callback([&svc](DataSourceID dsid) {
    DLOG("[unix_test.cc] Data source connected, id=%" PRIu64 "\n", dsid);
    DataSourceConfig config{"org.chromium.trace_events", "foo,bar"};
    svc->CreateDataSourceInstanceForTesting(config, 42);
  });
  svc->Start();
  task_runner.Run();
  return 0;
}

int ProducerMain() {
  PoorManTaskRunner task_runner;
  TestProducer producer;
  std::unique_ptr<Service> svc = UnixServiceConnection::ConnectAsProducer(
      kServiceSocketName, &producer, &task_runner);
  if (!svc) {
    perror("Could not connect producer");
    return 1;
  }
  producer.service = svc.get();
  task_runner.PostTask([&svc]() {
    DLOG("[unix_test.cc] Registering data source\n");
    DataSourceDescriptor desc{"org.chromium.trace_events"};
    auto callback = [](DataSourceID dsid) {
      printf("Data source registered with id=%" PRIu64 "\n", dsid);
    };
    svc->RegisterDataSource(42, desc, callback);
  });
  task_runner.Run();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "producer") == 0)
    return ProducerMain();

  if (argc == 2 && strcmp(argv[1], "service") == 0)
    return ServiceMain();

  printf("Usage: %s producer | service\n", argv[0]);
  return 1;
}
