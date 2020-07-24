/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>

#include "storybot.h"
#include "sence_mac_ota.h"
#include "yitian/pcbatest.h"
#include "driver/chip/hal_wakeup.h"
static OS_Thread_t poweroff_thread = {NULL};
static OS_Thread_t sleep_thread = {NULL};
static int poweroffflag = 0;


int idle_sleep_tmr = (30 * 60);		//15 mins
player_base *player;
player_base *player14;
scene_manager *scene;

char lock_status = 0;
char poweroffSflag = 0;
#ifdef __AP_LINK
char apstartflag = 0;
#endif

button_handle *orign_lang_button;
button_handle *target_lang_button;
button_handle *aichat_button;
button_handle *volume_up_button;
button_handle *volume_down_button;

button_handle *smartlink_button;
button_handle *poweroff_button;

uint8_t g_current_volume = 26;
int chk_aichat_scence = false;
int charging_state = 0;
#ifdef __SMARTLINK
int lingflag;//开机按AI启动配网
#endif
/* the state of voice and wechat button need record in any time ,
   so can't stop the voice and wechat button */
int translation_button_state = INVALID_STA;
int wechat_button_state = INVALID_STA;

uint8_t link_status = 0;		//connect status announce!!
uint8_t chrg_announce_flag = 0;		//charging status announce!!!
Tmrcnt_Handle *tmrcnt_heart_beat;
Tmrcnt_Handle *TimerSleep;

Tmrcnt_Handle *VolMonitor_Handle;
int VolMonitor_max = 0;


#define TIMERCHARGE  120
Tmrcnt_Handle *TimerCharge = NULL;
int chargeSceneflag = 0;

int get_cur_vol_level(void)
{
//	return ((g_current_volume - INTERVAL_VOLUME)/INTERVAL_VOLUME);
   int ret = (g_current_volume - MIN_VOLUME)/INTERVAL_VOLUME;
   if(ret > 5) ret = 5;
   if(ret < 1) ret = 1;
   return ret;
}

void set_cur_vol_level(int level)
{
	if(level != get_cur_vol_level()) {
		g_current_volume = level * INTERVAL_VOLUME + MIN_VOLUME;
		if(g_current_volume>MAX_VOLUME){
			g_current_volume = MAX_VOLUME;
		}
		player14->setvol(player14, g_current_volume);
	}
	#if 1
	struct sysinfo *info = sysinfo_get();
	if(player14->get_status(player14) == APLAYER_STATES_STOPPED) {
		player14->set_callback(player14, NULL, NULL);
		player14->play(player14, SCALE_TONE_00);
		while(player14->get_status(player14) != APLAYER_STATES_STOPPED) {
			OS_MSleep(100);
		}
	}
	info->volume = g_current_volume;
	sysinfo_save();
	#endif
}

void set_vol_ctrl_arg(int arg)
{
	int	cur_vol_level = get_cur_vol_level();
	if(VOL_UP == arg) {
		cur_vol_level = cur_vol_level + 1;
	} else if(VOL_DOWN == arg) {
		cur_vol_level = cur_vol_level - 1;
	}
	if(cur_vol_level < 1) cur_vol_level = 1;
	if(cur_vol_level > 5) cur_vol_level = 5;

	if(get_link_status()){
		handle_volume_msg(cur_vol_level, MSG_SET);
	}
}

void timer_sleep_callback(void);
void delete_vcc_sys()
{
	uint32_t bat_vol = -1;
	int ret;
	
	while(bat_vol == -1)
	{
		bat_vol = PM_get_new_vol();
		OS_MSleep(100);
	}
	printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@bat_vol:%d\n",bat_vol);
	if(bat_vol < 3300)
	{
		led_setting(STATUS_BLINK,LED_WHITE_CONTROL,5000,300,300);
		OS_MSleep(500);
		led_setting(STATUS_BLINK,LED_RED_CONTROL,5000,300,300);
		OS_MSleep(2000);
		led_setting(STATUS_BLINK,LED_RED_CONTROL,0,500,500);
		Drv_Rgb_Led_DeInit();
		
		buttons_deinit();
		buttons_low_level_deinit();
			
	//	buttons_deinit();
	//	HAL_Wakeup_SetTimer_mS(10000);
		HAL_Wakeup_SetIO(2, 1, 0);
		ret = wlan_sta_disable();
		if(ret != 0)
			printf("powerlow wlan sta disable failed!ret = %d\n",ret);
		
		ret = net_sys_stop();
		if(ret != 0)
			printf("powerlow net sys stop failed!ret = %d\n",ret);
			
		ret = pm_enter_mode(PM_MODE_POWEROFF);
		if(ret != 0)
		{
			printf("powerlow poweroff error! ret = %d\n",ret);	
		}	
	}
}
void PM_init();

OS_Thread_t record_socket_test_thread;
void record_socket_test(void *arg)
{
	OS_Sleep(15);
	while(1){
		sys_event_send(CTRL_MSG_TYPE_VKEY, 0, 2, 0);
		OS_Sleep(5);
		sys_event_send(CTRL_MSG_TYPE_VKEY, 1, 2, 0);
		OS_Sleep(10);
	}
}
/*
static void charging_tone_cb(player_events event, void *data, void *arg)
{
	int ret = *(int *)arg;
	if (event == PLAYER_EVENTS_MEDIA_PLAYBACK_COMPLETE) {
		printf("ret=%d+++++++++++++%s:%d\n",ret, __func__, __LINE__);
		if(ret == 0) {
			send_msg_to_aichat();
		}
	}
}
*/
void qizhi_ctrlcall_lowpwoer()
{
	int ret = -1;
	
		ret = get_new_percentage();
	printf("+++++++++++++%s:%d, get_new_percentage()=%d\n",__func__,__LINE__, get_new_percentage());
	if(ret <= 10)
		led_setting(STATUS_BLINK, LED_RED_CONTROL, 5000, 500, 500);
	else
		led_setting(STATUS_BLINK, LED_RED_CONTROL, 0, 500, 500);
}

void qizhi_ctrlcall_low_poweroff()
{
	
	printf("+++++++++++++%s:%d\n",__func__,__LINE__);

}

void timer_charge_callback(void)
{
//	printf("timer_charge_callback:chargeSceneflag->%d charging_state->%d\n",chargeSceneflag,charging_state);
	if(chargeSceneflag == 0 && charging_state == 1) {
		printf("scene --->STORYBOT_SCENES_CHRG\n");
		scene->request(scene, STORYBOT_SCENES_CHRG, (uint32_t)NULL);
	}
}

void qizhi_ctrlcall_charging()
{
	//printf("+++++++++++++%s:%d\n",__func__,__LINE__);
	led_setting(STATUS_GENERAL, LED_YELLOW_CONTROL, 4000, 2000, 200);
	charging_state = 1;
	timer_set_no_sleep();

	TimerCharge->set_callback(TimerCharge, timer_charge_callback, TIMERCHARGE, NULL);
	TimerCharge->tmrcnt_reset(TimerCharge);
}

void qizhi_ctrlcall_discharged()
{
	printf("%s:%d, get_new_percentage=%d\n", __func__, __LINE__, get_new_percentage());
	led_setting(STATUS_GENERAL, LED_YELLOW_CONTROL, 0, 500, 500);
	led_setting(STATUS_GENERAL, LED_GREEN_CONTROL, 0, 500, 500);
	charging_state = 0;
	timer_set_sleep();
}

void qizhi_ctrlcall_charged_full()
{
	charging_state = 1;
	printf("+++++++++++++%s:%d\n",__func__,__LINE__);
	led_setting(STATUS_GENERAL, LED_GREEN_CONTROL, 5000, 500, 500);
}

#ifdef __SMARTLINK
static void start_dialog_key_cb(BUTTON_STATE sta,void *arg)
{
	
	if(sta == PRESS) {
	printf("++++++++++++++++++++++++++++++++++++++++++++++++++++start_dialog_key_cb\n");	
	lingflag = 1;
		//	scene->request(scene, STORYBOT_SCENES_EASYLINK, (uint32_t)NULL);
		aichat_button->stop(aichat_button);
		aichat_button->cb = NULL;
	}
}
#endif

static void poweroff_start_dialog_key_cb(BUTTON_STATE sta,void *arg)
{
	printf("++++++++++++++++++++++++poweroff_start_dialog_key_cb\n");	
	if(sta == PRESS) {
		poweroffSflag = 1;
		//	scene->request(scene, STORYBOT_SCENES_EASYLINK, (uint32_t)NULL);
		target_lang_button->stop(target_lang_button);
		target_lang_button->cb = NULL;
	}
}

#ifdef __AP_LINK
static void ap_start_key_cb(BUTTON_STATE sta,void *arg)
{
	printf("+++++++++++++++++++++++++++++++++++++++ap_start_key_cb\n");	
	if(sta == PRESS) {
		apstartflag = 1;
		//	scene->request(scene, STORYBOT_SCENES_EASYLINK, (uint32_t)NULL);
		orign_lang_button->stop(orign_lang_button);
		orign_lang_button->cb = NULL;
	}
}
#endif 

static void VolMonitor_Handle_func(void)
{
	struct sysinfo *info = sysinfo_get();
	int bat_Volnew;

	printf("VolMonitor_Handle_func:volume:%d\n",info->volume);	
	bat_Volnew = PM_get_new_vol();
	if(VolMonitor_max == 0 )
	{
		VolMonitor_max = bat_Volnew;
		return;
	}else{
		if(info->volume == 28 || info->volume == 26)
		{	
			if((bat_Volnew >= 3800 && VolMonitor_max <= 3800) \
				 || (bat_Volnew <= 3800 && VolMonitor_max >= 3800))
			{
				player14->setvol(player14, 28);
			}
		}
		VolMonitor_max = bat_Volnew;
	}
}

static void storybot_init()
{
	topqizhi_tone_init();

	//AD_ButtonInit(button_drv_cb, NULL);
	struct sysinfo *info = sysinfo_get();
	if(info->volume != 0) {
		g_current_volume = info->volume;
	}

	powermanager_setcallback_low_power(qizhi_ctrlcall_lowpwoer);
	powermanager_setcallback_low_poweroff(qizhi_ctrlcall_low_poweroff);
	powermanager_setcallback_charging(qizhi_ctrlcall_charging);
	powermanager_setcallback_discharged(qizhi_ctrlcall_discharged);
	powermanager_setcallback_charged_full(qizhi_ctrlcall_charged_full);

	if(!((info->target_lang[0] <= 'z') && (info->target_lang[0] >= 'a'))) {
		printf("init trans lang\n");
#ifdef _BOARD_ZH
		memcpy(info->target_lang, "en-US", 6);
#else
		memcpy(info->target_lang, "cmn-Hans-CN", 12);
#endif
		sysinfo_save();
	} else {
		lang_init_set(TRANS_LANG);
	}

	if(!((info->src_lang[0] <= 'z') && (info->src_lang[0] >= 'a'))) {
		printf("init src lang\n");
#ifdef _BOARD_ZH
		memcpy(info->src_lang, "cmn-Hans-CN", 12);
#else
	//	memcpy(info->src_lang, "ja-JP", 5);
	memcpy(info->src_lang, "en-US", 6);
#endif
		sysinfo_save();
	} else {
		lang_init_set(SRC_LANG);
	}
	printf("info->cur_wlan_index=%d\n", info->cur_wlan_index);
	if (info->wlan_sta_param[0].ssid_len == 0 || info->wlan_sta_param[0].ssid_len > SYSINFO_SSID_LEN_MAX)
	{
		memcpy(info->wlan_sta_param[0].ssid, "yitian", 7);
		info->wlan_sta_param[0].ssid_len = strlen((char *)info->wlan_sta_param[0].ssid);
		memcpy(info->wlan_sta_param[0].psk, "Ttrueasy", 9);
		info->cur_wlan_index = 0;
		sysinfo_save();
	}

	scene = scene_manager_create();
	player14 = player_create();
	player = player14;
	
	/* register the low level button interface */
	button_impl_t key_impl = {
		buttons_low_level_init,
		buttons_low_level_get_state,
		buttons_low_level_wait_semaphore,
		buttons_low_level_release_semaphore,
		buttons_low_level_deinit
	};
	/* set buttons thread priority and stack size */
	buttons_set_thread_priority(4);
	buttons_set_thread_stack_size(1024);

	/* init buttons, will init the low level buttons, ad buttons and gpio buttons */
	buttons_init(&key_impl);

	target_lang_button = create_long_button(ORIGN_LANG_BUTTON_ID, 50);
	orign_lang_button = create_long_button(TARGET_LANG_BUTTON_ID, 50);
	aichat_button = create_short_long_button(AICHAT_BUTTON_ID, 3000);//aichat//repeat
	
	volume_up_button = create_short_button(VOL_UP_BUTTON_ID);
	volume_down_button = create_short_long_button(VOL_DOWN_BUTTON_ID, 3000);//vol-//easylink

	smartlink_button = create_short_long_button(SMARTLINK_BUTTON_ID, 1000);
	poweroff_button = create_short_long_button(POWEROFF_BUTTON_ID, 600);
		
	player14->setvol(player14, g_current_volume);

	tmrcnt_heart_beat = timer_count_creat();
	set_tmrcnt_heart_beat_cb(tmrcnt_heart_beat);
	
	VolMonitor_Handle = timer_count_creat();
	VolMonitor_Handle->set_callback(VolMonitor_Handle, VolMonitor_Handle_func, 10, NULL);
	VolMonitor_Handle->tmrcnt_start(VolMonitor_Handle);
	
	set_efuse_mac_init();
#ifdef __SMARTLINK	
		aichat_button->stop(aichat_button);
		aichat_button->cb = start_dialog_key_cb;
		aichat_button->start(aichat_button);
#endif		
		target_lang_button->stop(target_lang_button);
		target_lang_button->cb = poweroff_start_dialog_key_cb;
		target_lang_button->start(target_lang_button);
#ifdef __AP_LINK		
		orign_lang_button->stop(orign_lang_button);
		orign_lang_button->cb = ap_start_key_cb;
		orign_lang_button->start(orign_lang_button);
#endif
		
#if 0
	if (!OS_ThreadIsValid(&record_socket_test_thread)) {
		if (OS_ThreadCreate(&record_socket_test_thread,
							  "test",
							  record_socket_test,
							  NULL,
							  OS_THREAD_PRIO_APP,
							  (1024 * 2)) != OS_OK) {
			SCENE_RECORD_DEBUG("record_socket_test thread create error\n");
		}
	}
#endif
}

void poweroff_ctrl()
{
	int ret;
	
	if(poweroffflag == 1)
		return;
		
	poweroffflag = 1;
	led_setting(STATUS_GENERAL, LED_RED_CONTROL, 5000, 500,500);
	OS_MSleep(2000);
	scene->request(scene, STORYBOT_SCENES_CHRG, (uint32_t)NULL);


	//	led_setting(STATUS_GENERAL, LED_GREEN_CONTROL, 5000, 500, 500);
	printf("\n**********************************\n");
	printf("POWER OFF0001\n");
	printf("**********************************\n");

//	topqizhi_record_logout();
	destroy_mqtt_thread();
	
	tmrcnt_heart_beat->tmrcnt_exit(tmrcnt_heart_beat);

	TimerCharge->tmrcnt_exit(TimerCharge);

	TimerSleep->tmrcnt_exit(TimerSleep);
	
//	OS_MSleep(2500);
	led_setting(STATUS_GENERAL, LED_RED_CONTROL, 0, 500, 500);

	Drv_Rgb_Led_DeInit();
	
	buttons_deinit();
	buttons_low_level_deinit();
		
//	buttons_deinit();
//	HAL_Wakeup_SetTimer_mS(10000);
	HAL_Wakeup_SetIO(2, 1, 0);
	ret = wlan_sta_disable();
	if(ret != 0)
		printf("poweroff wlan sta disable failed!ret = %d\n",ret);
	
	ret = net_sys_stop();
	if(ret != 0)
		printf("poweroff net sys stop failed!ret = %d\n",ret);
		
	ret = pm_enter_mode(PM_MODE_POWEROFF);
	if(ret != 0)
	{
		printf("sleep start error! ret = %d\n",ret);	
	}	
}
void poweroff_run_thread(void *arg)
{
	poweroff_ctrl();
	OS_ThreadDelete(&poweroff_thread);
}
void poweroff_cb(BUTTON_STATE sta, void *arg)
{
	printf("\n**********************************\n");
	printf("POWER OFF\n");
	printf("**********************************\n");
	
			if (OS_ThreadCreate(&poweroff_thread,
							  "poweroff",
							  poweroff_run_thread,
							  0,
							  OS_THREAD_PRIO_APP,
							  1024) != OS_OK) {
			printf("poweroff thread create error");
		}
}

void sleep_run_thread(void *arg)
{
	poweroff_ctrl();
	OS_ThreadDelete(&sleep_thread);
}

//定时休眠
void timer_sleep_callback(void)
{	
//	printf("\n**********************************\n");
//	printf("timer_sleep_callback\n");
//	printf("**********************************\n");

	
	if(charging_state == 1) {
		timer_set_no_sleep();
			return;
		}
		
	if (OS_ThreadCreate(&sleep_thread,
							  "Psleep",
							  sleep_run_thread,
							  NULL,
							  OS_THREAD_PRIO_APP,
							  1024) != OS_OK) {
			printf("timer_sleep_callback thread create error");
		}
}
void timer_set_no_sleep()
{	
	TimerCharge->set_callback(TimerCharge, timer_charge_callback, 65000, NULL); //设置定时120s  抬起开启定时
	TimerCharge->tmrcnt_reset(TimerCharge);
	
	TimerSleep->set_callback(TimerSleep, timer_sleep_callback, 65000, NULL); //设置定时65000s 按下结束定时
	TimerSleep->tmrcnt_reset(TimerSleep);	
}
void timer_set_sleep()
{
	if(charging_state == 1) {
		timer_set_no_sleep();
		TimerCharge->set_callback(TimerCharge, timer_charge_callback, TIMERCHARGE, NULL);
		TimerCharge->tmrcnt_reset(TimerCharge);
			return;
		}

	TimerCharge->set_callback(TimerCharge, timer_charge_callback, TIMERCHARGE, NULL); 
	TimerCharge->tmrcnt_reset(TimerCharge);
	

	TimerSleep->set_callback(TimerSleep, timer_sleep_callback, POWEROFFTIME, NULL); //设置定时120s  抬起开启定时
	TimerSleep->tmrcnt_reset(TimerSleep);
}

int timer_sleep_init()
{
		TimerSleep = timer_count_creat();
		TimerSleep->set_callback(TimerSleep, timer_sleep_callback, POWEROFFTIME, NULL); //设置定时120s
		TimerSleep->tmrcnt_start(TimerSleep);
		
		TimerCharge = timer_count_creat();
		TimerCharge->set_callback(TimerCharge, timer_charge_callback, TIMERCHARGE, NULL);
		TimerCharge->tmrcnt_start(TimerCharge);	
	
		timer_set_sleep();
		return 0;
}
int main(void)
{
	platform_init();
	pcbatest_main();
	Led_InitCtrl();
	PM_init();
//	console_stop();
//	Machinestatus_init();
	timer_sleep_init();
	storybot_init();
	delete_vcc_sys();
	
	scene_base *scene_start = storybot_scene_start_create();
	scene_base *scene_aichat = storybot_scene_aichat_create();
#ifdef __SMARTLINK
	scene_base *scene_easy_link = storybot_scene_easylink_create();
#endif
	scene_base *scene_charging = storybot_scene_charging_create();
	scene_base *scene_ota = storybot_ota_create();
#ifdef __AP_LINK
	scene_base *scene_aplink = storybot_scene_aplink_create();
#endif
	scene_base *scene_mac_ota = mac_ota_aichat_create();
	
	scene->add(scene, scene_start);
	scene->add(scene, scene_aichat);
#ifdef __SMARTLINK
	scene->add(scene, scene_easy_link);
#endif
	scene->add(scene, scene_charging);
	scene->add(scene, scene_ota);
#ifdef __AP_LINK
	scene->add(scene, scene_aplink);
#endif
	scene->add(scene, scene_mac_ota);
	scene->run(scene);
	return 0;
}

