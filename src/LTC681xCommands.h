#pragma once

class LTC681xCommand {
 public:
  virtual uint16_t toValue() const = 0;
};

enum class AdcMode : uint8_t {
  k422 = 0,
  k27k = 1,
  k7k = 2,
  k26 = 3,
  k1k = 0,
  k14k = 1,
  k3k = 2,
  k2k = 3
};
enum class CellSelection : uint8_t {
  kAll = 0,
  k1 = 1, // 1, 7, 13
  k2 = 2, // 2, 8, 14
  k3 = 3, // 3, 9, 15
  k4 = 4, // 4, 10, 16
  k5 = 5, // 5, 11, 17
  k6 = 6 // 6, 12, 18
};
enum class PullDirection : uint8_t { kPullDown = 0, kPullUp = 1 };
enum class SelfTestMode : uint8_t { kSelfTest1 = 1, kSelfTest2 = 2 };
enum class GpioSelection : uint8_t {
  kAll = 0,
  k1 = 1,
  k2 = 2,
  k3 = 3,
  k4 = 4,
  k5 = 5,
  kRef = 6
};
enum class StatusGroupSelection : uint8_t {
  kAll = 0,
  k1 = 1, // SC
  k2 = 2, // ITMP
  k3 = 3, // VA
  k4 = 4  // VD
};

class WriteConfigurationGroupA : public LTC681xCommand {
  uint16_t toValue() const { return 0x0001; }
};
class WriteConfigurationGroupB : public LTC681xCommand {
  uint16_t toValue() const { return 0x0024; }
};
class ReadConfigurationGroupA : public LTC681xCommand {
  uint16_t toValue() const { return 0x0002; }
};
class ReadConfigurationGroupB : public LTC681xCommand {
  uint16_t toValue() const { return 0x0025; }
};

class ReadCellVoltageGroupA : public LTC681xCommand {
  uint16_t toValue() const { return 0x0004; }
};
class ReadCellVoltageGroupB : public LTC681xCommand {
  uint16_t toValue() const { return 0x0006; }
};
class ReadCellVoltageGroupC : public LTC681xCommand {
  uint16_t toValue() const { return 0x0008; }
};
class ReadCellVoltageGroupD : public LTC681xCommand {
  uint16_t toValue() const { return 0x000A; }
};
class ReadCellVoltageGroupE : public LTC681xCommand {
  uint16_t toValue() const { return 0x0009; }
};
class ReadCellVoltageGroupF : public LTC681xCommand {
  uint16_t toValue() const { return 0x000B; }
};

class ReadAuxiliaryGroupA : public LTC681xCommand {
  uint16_t toValue() const { return 0x000C; }
};
class ReadAuxiliaryGroupB : public LTC681xCommand {
  uint16_t toValue() const { return 0x000E; }
};
class ReadAuxiliaryGroupC : public LTC681xCommand {
  uint16_t toValue() const { return 0x000D; }
};
class ReadAuxiliaryGroupD : public LTC681xCommand {
  uint16_t toValue() const { return 0x000F; }
};

class ReadStatusGroupA : public LTC681xCommand {
  uint16_t toValue() const { return 0x0010; }
};
class ReadStatusGroupB : public LTC681xCommand {
  uint16_t toValue() const { return 0x0012; }
};

class WriteSControlGroup : public LTC681xCommand {
  uint16_t toValue() const { return 0x0016; }
};
class WritePWMGroup : public LTC681xCommand {
  uint16_t toValue() const { return 0x0020; }
};
class ReadSControlGroup : public LTC681xCommand {
  uint16_t toValue() const { return 0x0016; }
};
class ReadPWMGroup : public LTC681xCommand {
  uint16_t toValue() const { return 0x0022; }
};

class StartSControlPulsing : public LTC681xCommand {
  uint16_t toValue() const { return 0x0019; }
};
class ClearSControlGroup : public LTC681xCommand {
  uint16_t toValue() const { return 0x0018; }
};
class PollADC : public LTC681xCommand {
  uint16_t toValue() const { return 0x0714; }
};

class StartCellVoltageADC : public LTC681xCommand {
 public:
  StartCellVoltageADC(AdcMode a, bool d, CellSelection c)
      : adcMode(a), dischargePermitted(d), cellSelection(c) {}
  AdcMode adcMode;
  bool dischargePermitted;
  CellSelection cellSelection;
  uint16_t toValue() const {
    return 0x0260 | (((uint16_t)adcMode) << 7) |
           (dischargePermitted ? 0x0010 : 0x0000) | ((uint16_t)cellSelection);
  }
};
class StartOpenWireADC : public LTC681xCommand {
 public:
  AdcMode adcMode;
  PullDirection pull;
  bool dischargePermitted;
  CellSelection cellSelection;
  uint16_t toValue() const {
    return 0x0228 | ((uint16_t)adcMode << 7) | ((uint16_t)pull << 6) |
           (dischargePermitted ? 0x0010 : 0x0000) | ((uint16_t)cellSelection);
  }
};
class StartSelfTestConversion : public LTC681xCommand {
 public:
  AdcMode adcMode;
  SelfTestMode testMode;
  uint16_t toValue() const {
    return 0x0207 | ((uint16_t)adcMode << 7) | ((uint16_t)testMode << 5);
  }
};
// Lost my mind at "Start Overlap Measurement of Cell 7 Voltage"
class StartGpioADC : public LTC681xCommand {
 public:
  StartGpioADC(AdcMode a, GpioSelection g) : adcMode(a), gpioSelection(g) {}
  AdcMode adcMode;
  GpioSelection gpioSelection;
  uint16_t toValue() const {
    return 0x0460 | ((uint16_t)adcMode << 7) | ((uint16_t)gpioSelection);
  }
};
// Lost my mind at "Start Overlap Measurement of Cell 7 Voltage"
class StartStatusADC : public LTC681xCommand {
 public:
  StartStatusADC(AdcMode a, StatusGroupSelection s) : adcMode(a), statusSelection(s) {}
  AdcMode adcMode;
  StatusGroupSelection statusSelection;
  uint16_t toValue() const {
    return 0x0468 | ((uint16_t)adcMode << 7) | ((uint16_t)statusSelection);
  }
};
class StartCombinedADC : public LTC681xCommand {
 public:
  StartCombinedADC(AdcMode a) : adcMode(a) {}
  AdcMode adcMode;
  uint16_t toValue() const {
    return 0x046F | ((uint16_t)adcMode << 7);
  }
};

class MuteDischarge : public LTC681xCommand {
 public:
  uint16_t toValue() const {
    return 0x28;
  }
};
class UnmuteDischarge : public LTC681xCommand {
 public:
  uint16_t toValue() const {
    return 0x29;
  }
};
