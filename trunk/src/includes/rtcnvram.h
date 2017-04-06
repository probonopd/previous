int rtc_interface_io(Uint8 rtdatabit);
void rtc_interface_reset(void);

void rtc_request_power_down(void);
void rtc_stop_pdown_request(void);

void nvram_init(void);
void nvram_checksum(int force);
char * get_rtc_ram_info(void);