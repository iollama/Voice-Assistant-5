#pragma once

#ifdef ESP32

#include "../../internals/ws_common.hpp"
#include "../tcp_client.hpp"
#include "../tcp_server.hpp"
#include "../generic_esp/generic_esp_clients.hpp"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace websockets { namespace network {
  typedef GenericEspTcpClient<WiFiClient> Esp32TcpClient;
  
  class SecuredEsp32TcpClient : public GenericEspTcpClient<WiFiClientSecure> {
  public:

    // Udi, 2026-04-02: fix missing setInsecure() for ESP32 - without this,
    // upgradeToSecuredConnection() never disables cert verification and WSS fails.
    void setInsecure() {
      this->client.setInsecure();
    }
    // end fix

    void setCACert(const char* ca_cert) {
      this->client.setCACert(ca_cert);
    }

    void setCertificate(const char* client_ca) {
      this->client.setCertificate(client_ca);
    }
    
    void setPrivateKey(const char* private_key) {
      this->client.setPrivateKey(private_key);
    }    
  };


  class Esp32TcpServer : public TcpServer {
  public:
    Esp32TcpServer() {}
    bool poll() override {
      yield();
      return server.hasClient();
    }

    bool listen(const uint16_t port) override {
      yield();
      server = WiFiServer(port);
      server.begin(port);
      return available();
    }
    
    TcpClient* accept() override {
      while(available()) {
        auto client = server.available();
        if(client) {
          return new Esp32TcpClient{client};
        }
      }
      return new Esp32TcpClient;
    }

    bool available() override {
      yield();
      return static_cast<bool>(server);
    }
    
    void close() override {
      yield();
      server.close();
    }

    virtual ~Esp32TcpServer() {
      if(available()) close();
    }

  protected:
    int getSocket() const override {
      return -1; // Not Implemented
    }
  
  private:
    WiFiServer server;
  };
}} // websockets::network

#endif // #ifdef ESP32 
