#pragma once

class Telemetry {
   public:
    Telemetry();
    bool begin();
    void sendStatus(const char* payload);
};
