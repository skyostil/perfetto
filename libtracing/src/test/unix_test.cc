#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <list>
#include <map>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/data_source_descriptor.h"
#include "libtracing/core/producer.h"
#include "libtracing/core/service.h"
#include "libtracing/core/shared_memory.h"
#include "libtracing/core/task_runner.h"
#include "libtracing/src/core/base.h"
#include "libtracing/src/test/test_task_runner.h"
#include "libtracing/transport/service_proxy_for_producer.h"
#include "libtracing/unix_transport/unix_service_connection.h"
#include "libtracing/unix_transport/unix_service_host.h"

#include "gmock/gmock.h"

namespace {

using namespace perfetto;
using namespace testing;

const char kServiceSocketName[] = "/tmp/perfetto_test_sock";

class TestProducer : public Producer {
 public:
  void CreateDataSourceInstance(DataSourceInstanceID id,
                                const DataSourceConfig& conf) override {
    DLOG(
        "[unix_test.cc] CreateDataSourceInstance name=%s filters=%s "
        "instance_id=%" PRIu64 "\n",
        conf.data_source_name.c_str(), conf.trace_category_filters.c_str(), id);
  }

  void TearDownDataSourceInstance(DataSourceInstanceID id) override {}

  void OnConnect() override {
    DLOG("[unix_test.cc] OnConnect() ");
    DCHECK(service_proxy);
    SharedMemory* shm = service_proxy->GetSharedMemory();
    DCHECK(shm);
    DLOG("[unix_test.cc] Succesfully wrote to the shared memory\n");
    memcpy(shm->start(), "bazinga", 8);
    service_proxy->NotifyPageReleased(1);
  }

  ServiceProxyForProducer* service_proxy = nullptr;
};

class TestServiceObserver : public UnixServiceHost::ObserverForTesting {
 public:
  // TestServiceObserver() {}
  // ~TestServiceObserver() override {}
  MOCK_METHOD1(OnProducerConnected, void(ProducerID));
  MOCK_METHOD1(OnDataSourceRegistered, void(DataSourceID));
  MOCK_METHOD1(OnDataSourceUnregistered, void(DataSourceID));
  MOCK_METHOD1(OnDataSourceInstanceCreated, void(DataSourceInstanceID));
  MOCK_METHOD1(OnDataSourceInstanceDestroyed, void(DataSourceInstanceID));
};

int ServiceMain() {
  unlink(kServiceSocketName);

  TestServiceObserver observer;
  TestTaskRunner task_runner;
  std::unique_ptr<UnixServiceHost> svc_host = UnixServiceHost::CreateInstance(
      kServiceSocketName, &task_runner, &observer);

  if (!svc_host) {
    perror("Could not create service.");
    return 1;
  }

  EXPECT_CALL(observer, OnProducerConnected(_))
      .WillRepeatedly(Invoke([&svc_host](ProducerID prid) {
        DLOG("[unix_test.cc] Producer connected, id=%" PRIu64 "\n", prid);
        DataSourceConfig config{"org.chromium.trace_events", "foo,bar"};
        svc_host->service_for_testing()->CreateDataSourceInstanceForTesting(
            prid, config);
      }));

  EXPECT_CALL(observer, OnDataSourceRegistered(_))
      .WillRepeatedly(Invoke([](DataSourceID dsid) {
        DLOG("[unix_test.cc] OnDataSourceRegistered, id=%" PRIu64 "\n", dsid);

      }));

  svc_host->Start();
  task_runner.Run();
  return 0;
}

int ProducerMain() {
  TestTaskRunner task_runner;
  TestProducer producer;
  std::unique_ptr<ServiceProxyForProducer> service_proxy =
      UnixServiceConnection::ConnectAsProducer(kServiceSocketName, &producer,
                                               &task_runner);
  if (!service_proxy) {
    perror("Could not connect producer");
    return 1;
  }
  producer.service_proxy = service_proxy.get();
  task_runner.PostTask([&service_proxy]() {
    DLOG("[unix_test.cc] Registering data source\n");
    DataSourceDescriptor desc{"org.chromium.trace_events"};
    auto callback = [](DataSourceID dsid) {
      printf("Data source registered with id=%" PRIu64 "\n", dsid);
    };
    service_proxy->RegisterDataSource(desc, callback);
  });
  task_runner.Run();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);

  if (argc == 2 && strcmp(argv[1], "producer") == 0)
    return ProducerMain();

  if (argc == 2 && strcmp(argv[1], "service") == 0)
    return ServiceMain();

  printf("Usage: %s producer | service\n", argv[0]);
  return 1;
}
