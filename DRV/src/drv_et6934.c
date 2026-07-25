#include "drv_et6934.h"
#include "drv_iic.h"

static ET6934_Handle_t screen1;
static ET6934_Handle_t screen2;
static ET6934_Handle_t screen3;

// 为了方便循环操作，搞个指针数组指向它们
static ET6934_Handle_t *screen_ptrs[3] = {&screen1, &screen2, &screen3};

// 虚拟显存：3个屏幕，每个屏幕16个GRID，每个GRID 1个字节 (对应SEG1~8)
static uint8_t disp_ram[3][16];

// 标准7段数码管字模 (0-9)，对应 bit0~6 为 a~g
static const uint8_t font_7seg[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
									  0x6D, 0x7D, 0x07, 0x7F, 0x6F};
// ================= 底层基础绘图函数 =================
// 设置任意单个映射点 (1 ~ 384)
static void DRC_SetPixel(uint16_t point_id, bool state)
{
	if (point_id < 1 || point_id > 384)
		return;

	uint16_t idx = point_id - 1;		// 转换为 0~383 索引
	uint8_t scr_idx = idx / 128;		// 计算所在屏幕 (0, 1, 2)
	uint8_t grid_idx = (idx % 128) / 8; // 计算 GRID (0 ~ 15)
	uint8_t seg_bit = idx % 8;			// 计算 SEG 位 (0 ~ 7)

	if (state)
	{
		disp_ram[scr_idx][grid_idx] |= (1 << seg_bit);
	}
	else
	{
		disp_ram[scr_idx][grid_idx] &= ~(1 << seg_bit);
	}
}

// 批量设置多个点 (用于一个段对应多个物理引脚的情况)
static void DRC_SetPixelList(const uint16_t *list, uint8_t count, bool state)
{
	for (uint8_t i = 0; i < count; i++)
	{
		DRC_SetPixel(list[i], state);
	}
}
// ---------------- 辅助函数 ----------------
// 专门用于处理一个段对应多个点位的情况（遇到 0 结束遍历）
static void DRC_SetSeg(const uint16_t *list, bool state)
{
	if (!list)
		return;
	for (uint8_t i = 0; list[i] != 0; i++)
	{
		DRC_SetPixel(list[i], state);
	}
}

// ================= 数据映射表 (依据您的测试数据提取) =================

// 转速条映射 (0~39 共40个点)
static const uint16_t MAP_RPM[40] = {48,  45,  41,	44,	 43,  42,  90,	91,	 92,  89,
									 93,  96,  94,	95,	 103, 102, 104, 101, 97,  100,
									 99,  98,  209, 216, 215, 214, 210, 211, 213, 212,
									 204, 205, 203, 202, 206, 207, 208, 201, 241, 248};

// ---------------- 车速映射表 ----------------

// 1. 车速百位 (只有 b, c 段)
static const uint16_t MAP_SPEED_100_B[] = {61, 64, 0};
static const uint16_t MAP_SPEED_100_C[] = {62, 63, 0};

// 2. 车速十位 (a~g，每个段对应多个引脚)
static const uint16_t MAP_SPEED_10_A[] = {146, 147, 149, 0};
static const uint16_t MAP_SPEED_10_B[] = {163, 165, 0};
static const uint16_t MAP_SPEED_10_C[] = {161, 164, 0};
static const uint16_t MAP_SPEED_10_D[] = {152, 168, 176, 0};
static const uint16_t MAP_SPEED_10_E[] = {145, 148, 0};
static const uint16_t MAP_SPEED_10_F[] = {150, 151, 0};
static const uint16_t MAP_SPEED_10_G[] = {162, 166, 167, 0};

static const uint16_t *const MAP_SPEED_10[7] = {
	MAP_SPEED_10_A, MAP_SPEED_10_B, MAP_SPEED_10_C, MAP_SPEED_10_D,
	MAP_SPEED_10_E, MAP_SPEED_10_F, MAP_SPEED_10_G};

// 3. 车速个位 (a~g，每个段对应多个引脚)
static const uint16_t MAP_SPEED_1_A[] = {170, 171, 173, 0};
static const uint16_t MAP_SPEED_1_B[] = {154, 155, 0};
static const uint16_t MAP_SPEED_1_C[] = {153, 156, 0};
static const uint16_t MAP_SPEED_1_D[] = {160, 185, 192, 0};
static const uint16_t MAP_SPEED_1_E[] = {169, 172, 0};
static const uint16_t MAP_SPEED_1_F[] = {174, 175, 0};
static const uint16_t MAP_SPEED_1_G[] = {157, 158, 159, 0};

static const uint16_t *const MAP_SPEED_1[7] = {
	MAP_SPEED_1_A, MAP_SPEED_1_B, MAP_SPEED_1_C, MAP_SPEED_1_D,
	MAP_SPEED_1_E, MAP_SPEED_1_F, MAP_SPEED_1_G};

// 小计里程段码映射 (十万位~个位，每个位包含 a,b,c,d,e,f,g 的 point_id)
// 顺序严格按照：a, b, c, d, e, f, g
static const uint16_t MAP_TRIP_100K[7] = {286, 283, 287, 284, 288, 285, 281};
static const uint16_t MAP_TRIP_10K[7] = {294, 291, 295, 292, 296, 293, 289};
static const uint16_t MAP_TRIP_1K[7] = {302, 299, 303, 300, 304, 301, 297};
static const uint16_t MAP_TRIP_100[7] = {310, 307, 311, 308, 312, 309, 305};
static const uint16_t MAP_TRIP_10[7] = {350, 347, 351, 348, 352, 349, 345};
static const uint16_t MAP_TRIP_1[7] = {342, 339, 343, 340, 344, 341, 337};

static const uint16_t *const MAP_TRIP_DIGITS[6] = {
	MAP_TRIP_1, MAP_TRIP_10, MAP_TRIP_100, MAP_TRIP_1K, MAP_TRIP_10K, MAP_TRIP_100K};

// ---------------- 油量和电压映射 (0~4 级) ----------------
static const uint16_t MAP_FUEL[5][2] = {{85, 88}, {50, 51}, {49, 52}, {53, 56}, {54, 55}};

static const uint16_t MAP_BATTERY[5][2] = {
	{332, 336}, {329, 335}, {323, 325}, {143, 189}, {187, 188}};

// 画布
static const uint16_t canvas_points[] = {
	17,	 18,  19,  20,	21,	 22,  23,  24,	33,	 34,  35,  36,	37,	 38,  39,  40,	46,
	105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 47,
	57,	 58,  59,  60,	81,	 83,  84,  82,	121, 122, 123, 124, 125, 126, 127, 128, 137,
	142, 144, 249, 250, 251, 252, 253, 254, 255, 256, 138, 139, 140, 141, 186, 193, 194,
	195, 196, 197, 198, 199, 200, 242, 243, 244, 245, 246, 247, 257, 260, 263, 264, 265,
	267, 269, 272, 278, 313, 314, 315, 316, 317, 319, 320, 353, 355, 356, 358, 360, 266,
	270, 271, 322, 326, 268, 321, 324, 327, 328, 330, 334, 331, 333,
};

// ================= 核心 API 实现 =================

void DRC_ET6934_Init(void)
{
	DRV_IIC_Bus *bus1;
	DRV_IIC_Bus *bus2;
	DRV_IIC_Bus *bus3;

	DRV_IIC_InitBus(DRV_IIC_BUS_LED1);
	DRV_IIC_InitBus(DRV_IIC_BUS_LED2);
	DRV_IIC_InitBus(DRV_IIC_BUS_LED3);

	bus1 = DRV_IIC_GetBus(DRV_IIC_BUS_LED1);
	bus2 = DRV_IIC_GetBus(DRV_IIC_BUS_LED2);
	bus3 = DRV_IIC_GetBus(DRV_IIC_BUS_LED3);
	if (bus1 == 0 || bus2 == 0 || bus3 == 0)
	{
		return;
	}

	// 1. 调用您的 BSP 初始化函数
	ET6934_Init(&screen1, &bus1->handle, ET6934_ADDR_FIXED);
	ET6934_Init(&screen2, &bus2->handle, ET6934_ADDR_FIXED);
	ET6934_Init(&screen3, &bus3->handle, ET6934_ADDR_FIXED);

	// 2. 清空 DRC 层的虚拟显存
	DRC_ET6934_ClearAll();

	// 3. 将干净的显存推送到硬件
	DRC_ET6934_Refresh();
}

void DRC_ET6934_ClearAll(void)
{
	for (int s = 0; s < 3; s++)
	{
		for (int g = 0; g < 16; g++)
		{
			disp_ram[s][g] = 0x00;
		}
	}
}

// 全亮
void DRC_ET6934_SetAll(void)
{
	for (int s = 0; s < 3; s++)
	{
		for (int g = 0; g < 16; g++)
		{
			disp_ram[s][g] = 0xFF;
		}
	}
}

// 刷新显存到硬件
void DRC_ET6934_Refresh(void)
{
	//__disable_irq();
	for (int s = 0; s < 3; s++)
	{
		ET6934_Refresh_RAM(screen_ptrs[s], &disp_ram[s][0]);
	}
	//__enable_irq();
}

// 渲染静态画布
void DRC_ET6934_SetCanvas(bool state)
{
	DRC_SetPixelList(canvas_points, sizeof(canvas_points) / sizeof(canvas_points[0]),
					 state);
}

// 渲染独立指示灯
void DRC_ET6934_SetIndicator(DRC_Indicator_t ind, bool state)
{
	switch (ind)
	{
	case IND_WATER_TEMP:
		DRC_SetPixel(3, state);
		DRC_SetPixel(4, state);
		break;
	case IND_FAULT:
		DRC_SetPixel(5, state);
		break;
	case IND_START_STOP:
		DRC_SetPixel(8, state);
		break;
	case IND_ENGINE_FAULT:
		DRC_SetPixel(25, state);
		DRC_SetPixel(28, state);
		break;
	case IND_TURN_LEFT:
		DRC_SetPixel(26, state);
		DRC_SetPixel(27, state);
		break;
	case IND_HIGH_BEAM:
		DRC_SetPixel(29, state);
		DRC_SetPixel(32, state);
		break;
	case IND_P_GEAR:
		DRC_SetPixel(30, state);
		DRC_SetPixel(31, state);
		break;
	case IND_TCS:
		DRC_SetPixel(129, state);
		DRC_SetPixel(136, state);
		break;
	case IND_ABS:
		DRC_SetPixel(130, state);
		DRC_SetPixel(131, state);
		break;
	case IND_TURN_RIGHT:
		DRC_SetPixel(132, state);
		DRC_SetPixel(133, state);
		break;
	case IND_BATTERY_ALARM:
		DRC_SetPixel(134, state);
		DRC_SetPixel(135, state);
		break;
	case IND_SPEED_KMH:
		DRC_SetPixel(182, state);
		DRC_SetPixel(190, state);
		break;
	case IND_SPEED_MPH:
		DRC_SetPixel(183, state);
		DRC_SetPixel(191, state);
		break;
	case IND_ODO_MODE:
		DRC_SetPixel(259, state);
		break;
	case IND_TRIP_MODE:
		DRC_SetPixel(261, state);
		DRC_SetPixel(262, state);
		break;
	case IND_CLOCK_MODE:
		DRC_SetPixel(277, state);
		break;
	case IND_TRIP_UNIT_MILES:
		DRC_SetPixel(290, state);
		DRC_SetPixel(346, state);
		DRC_SetPixel(354, state);
		break;
	case IND_TRIP_UNIT_KM:
		DRC_SetPixel(338, state);
		break;
	case IND_SEC_JUMP_0:
		DRC_SetPixel(298, state);
		break;
	case IND_SEC_JUMP_1:
		DRC_SetPixel(306, state);
		break;
	case IND_BTN_SET:
		DRC_SetPixel(314, state);
		DRC_SetPixel(317, state);
		break;
	case IND_FUEL_WHITE:
		DRC_SetPixel(86, state);
		break;
	case IND_FUEL_YELLOW:
		DRC_SetPixel(87, state);
		break;
	default:
		break;
	}
}

// 渲染转速条 (0~39)
void DRC_ET6934_SetRPM(uint8_t rpm_level)
{
	if (rpm_level > 39)
		rpm_level = 39;
	for (uint8_t i = 0; i < 40; i++)
	{
		DRC_SetPixel(MAP_RPM[i], (i < rpm_level) ? true : false);
	}
}

// 渲染油量 (0~5)
void DRC_ET6934_SetFuel(uint8_t num_bars)
{
	if (num_bars > 5)
		num_bars = 5;
	for (uint8_t i = 0; i < 5; i++)
	{
		bool state = (i < num_bars) ? true : false;
		DRC_SetPixel(MAP_FUEL[i][0], state);
		DRC_SetPixel(MAP_FUEL[i][1], state);
	}
}

// 渲染电压/电池 (0~5格)
void DRC_ET6934_SetBattery(uint8_t num_bars)
{
	if (num_bars > 5)
		num_bars = 5;
	for (uint8_t i = 0; i < 5; i++)
	{
		// 修改为 < num_bars，这样传入 0 就是全灭，传入 1 就是亮 1 格
		bool state = (i < num_bars) ? true : false;
		DRC_SetPixel(MAP_BATTERY[i][0], state);
		DRC_SetPixel(MAP_BATTERY[i][1], state);
	}
}

// ---------------- 车速渲染函数 ----------------
void DRC_ET6934_SetSpeed(uint16_t speed)
{
	if (speed > 199)
		speed = 199;
	uint8_t h = speed / 100;
	uint8_t t = (speed / 10) % 10;
	uint8_t u = speed % 10;

	bool show_h = (h == 1);
	DRC_SetSeg(MAP_SPEED_100_B, show_h);
	DRC_SetSeg(MAP_SPEED_100_C, show_h);

	uint8_t font_t = font_7seg[t];
	if (speed < 10)
		font_t = 0x00;
	for (uint8_t seg = 0; seg < 7; seg++)
	{
		DRC_SetSeg(MAP_SPEED_10[seg], (font_t & (1 << seg)) ? true : false);
	}

	uint8_t font_u = font_7seg[u];
	for (uint8_t seg = 0; seg < 7; seg++)
	{
		DRC_SetSeg(MAP_SPEED_1[seg], (font_u & (1 << seg)) ? true : false);
	}
}

// 渲染小计位置的单个数位 (digit_idx: 0~5)
void DRC_ET6934_SetSingleDigit(uint8_t digit_idx, uint8_t value)
{
	if (digit_idx > 5)
		return;
	if (value > 9)
		return;

	uint8_t font = font_7seg[value];
	for (uint8_t seg = 0; seg < 7; seg++)
	{
		DRC_SetPixel(MAP_TRIP_DIGITS[digit_idx][seg], (font & (1 << seg)) ? true : false);
	}
}

// 渲染小计里程数
void DRC_ET6934_SetTrip(uint32_t trip_val)
{
	if (trip_val > 999999)
		trip_val = 999999;

	for (uint8_t digit_idx = 0; digit_idx < 6; digit_idx++)
	{
		uint8_t num = trip_val % 10;
		uint8_t font = font_7seg[num];

		// 遍历 a~g 7个段
		for (uint8_t seg = 0; seg < 7; seg++)
		{
			bool is_on = (font & (1 << seg)) ? true : false;
			DRC_SetPixel(MAP_TRIP_DIGITS[digit_idx][seg], is_on);
		}

		trip_val /= 10;
	}
}

// 小计位置全显/全灭
void DRC_ET6934_SetTripAll(bool state)
{
	for (uint8_t digit_idx = 0; digit_idx < 6; digit_idx++)
	{
		for (uint8_t seg = 0; seg < 7; seg++)
		{
			DRC_SetPixel(MAP_TRIP_DIGITS[digit_idx][seg], state);
		}
	}
}

// 在小计里程的万位 (MAP_TRIP_10K) 渲染字符 'P'
void DRC_ET6934_SetTrip10K_P(bool state)
{
	// 'P' 的 7 段字模：点亮 a, b, e, f, g -> 二进制 0111 0011 -> 0x73
	uint8_t font_p = 0x73;

	for (uint8_t seg = 0; seg < 7; seg++)
	{
		// 如果 state 为 true：点亮 'P' 对应的段，并强制关闭不需要的段 (c, d) 以防乱码
		// 如果 state 为 false：关闭该位置的所有 7 个段
		bool is_on = (state && (font_p & (1 << seg))) ? true : false;

		// MAP_TRIP_10K 数组已经按 a~g 的顺序排列，直接调用底层绘图
		DRC_SetPixel(MAP_TRIP_10K[seg], is_on);
	}
}

// 渲染时钟 (HH:MM)
void DRC_ET6934_SetClock(uint8_t hour, uint8_t minute)
{
	if (hour > 23)
		hour = 23;
	if (minute > 59)
		minute = 59;

	// 1. 清空所有 6 位小计位置，防止残影
	DRC_ET6934_SetTripAll(false);

	// 2. 分钟个位 (使用 INDEX 1)
	uint8_t m_u = minute % 10;
	for (uint8_t seg = 0; seg < 7; seg++)
	{
		DRC_SetPixel(MAP_TRIP_DIGITS[1][seg],
					 (font_7seg[m_u] & (1 << seg)) ? true : false);
	}
	// 3. 分钟十位 (使用 INDEX 2)
	uint8_t m_t = minute / 10;
	for (uint8_t seg = 0; seg < 7; seg++)
	{
		DRC_SetPixel(MAP_TRIP_DIGITS[2][seg],
					 (font_7seg[m_t] & (1 << seg)) ? true : false);
	}
	// 4. 小时个位 (使用 INDEX 3 - 冒号左侧)
	uint8_t h_u = hour % 10;
	for (uint8_t seg = 0; seg < 7; seg++)
	{
		DRC_SetPixel(MAP_TRIP_DIGITS[3][seg],
					 (font_7seg[h_u] & (1 << seg)) ? true : false);
	}
	// 5. 小时十位 (使用 INDEX 4)
	uint8_t h_t = hour / 10;
	if (h_t > 0)
	{
		for (uint8_t seg = 0; seg < 7; seg++)
		{
			DRC_SetPixel(MAP_TRIP_DIGITS[4][seg],
						 (font_7seg[h_t] & (1 << seg)) ? true : false);
		}
	}

	// 冒号由 IND_SEC_JUMP 控制，此处不强制操作像素
}
