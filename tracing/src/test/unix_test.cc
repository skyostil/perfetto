#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <list>
#include <map>

#include "tracing/core/data_source_config.h"
#include "tracing/core/data_source_descriptor.h"
#include "tracing/core/producer.h"
#include "tracing/core/service.h"
#include "tracing/core/shared_memory.h"
#include "tracing/core/task_runner.h"
#include "tracing/src/core/base.h"
#include "tracing/src/test/test_task_runner.h"
#include "tracing/src/unix_rpc/unix_shared_memory.h"
#include "tracing/unix_rpc/unix_service_connection.h"
#include "tracing/unix_rpc/unix_service_host.h"

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

  void OnConnect(ProducerID prid, SharedMemory* shared_memory) override {
    DLOG("[unix_test.cc] OnConnect() \n");
    DCHECK(service_endpoint);
    DLOG("[unix_test.cc] Succesfully wrote to the shared memory\n");
    memcpy(shared_memory->start(), "bazinga", 8);
    service_endpoint->NotifyPageReleased(1);
  }

  Service::ProducerEndpoint* service_endpoint = nullptr;
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
        DLOG("[unix_test.cc] Producer connected, id=%" PRIu64, prid);
        DataSourceConfig config{"org.chromium.trace_events", "foo,bar"};
        svc_host->service_for_testing()->CreateDataSourceInstanceForTesting(
            prid, config);
      }));

  EXPECT_CALL(observer, OnDataSourceRegistered(_))
      .WillRepeatedly(Invoke([](DataSourceID dsid) {
        DLOG("[unix_test.cc] OnDataSourceRegistered, id=%" PRIu64, dsid);

      }));

  svc_host->Start();
  task_runner.Run();
  return 0;
}

int ProducerMain() {
  TestTaskRunner task_runner;
  TestProducer producer;
  std::unique_ptr<Service::ProducerEndpoint> service_endpoint =
      UnixServiceConnection::ConnectAsProducer(kServiceSocketName, &producer,
                                               &task_runner);
  if (!service_endpoint) {
    perror("Could not connect producer");
    return 1;
  }
  producer.service_endpoint = service_endpoint.get();
  task_runner.PostTask([&service_endpoint]() {
    DLOG("[unix_test.cc] Registering data source\n");
    DataSourceDescriptor desc{"org.chromium.trace_events"};
    auto callback = [](DataSourceID dsid) {
      printf("Data source registered with id=%" PRIu64 "\n", dsid);
    };
    service_endpoint->RegisterDataSource(desc, callback);
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
