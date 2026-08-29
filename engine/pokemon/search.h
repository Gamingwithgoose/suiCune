void BeastsCheck(void);
void MonCheck(void);
bool CheckOwnMonAnywhere(SpeciesId species);
bool CheckOwnMon(const struct BoxMon* hl, const uint8_t* ot, SpeciesId species);
bool CheckOwnNativeMon(const struct NativeBoxMon* mon, const uint8_t* ot, SpeciesId species);
extern const uint32_t SearchBoxAddressTable[];
void UpdateOTPointer(void);
