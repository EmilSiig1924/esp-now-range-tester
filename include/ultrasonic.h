#pragma once

struct PingPacket;
void startUltrasonic();
void appendUltrasonicReading(PingPacket& packet);
