void rtc_init();
int rtc_get_time(time_t *time);
int rtc_get_time_local(time_t *time);
int rtc_get_temp(float *temp);
int rtc_set_time(const time_t time);
