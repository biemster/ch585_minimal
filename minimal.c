#include <stdint.h>
#include <string.h>
#include "CH585SFR.h"

#define SLEEPTIME_MS 300

#define __INTERRUPT   __attribute__((interrupt()))

#define SYS_SAFE_ACCESS(a)  do { R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1; \
								 R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2; \
								 asm volatile ("nop\nnop"); \
								 {a} \
								 R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG0; \
								 asm volatile ("nop\nnop"); } while(0)

// For debug writing to the debug interface.
#define DMDATA0 			   (*((PUINT32V)0xe0000380))

#define GPIO_Pin_8             (1<<8)
#define GPIOA_ResetBits(pin)   (R32_PA_CLR |= (pin))
#define GPIOA_SetBits(pin)     (R32_PA_OUT |= (pin))
#define GPIOA_ModeCfg_Out(pin) R32_PA_PD_DRV &= ~(pin); R32_PA_DIR |= (pin)

typedef struct {
	volatile uint32_t CTLR;
	volatile uint32_t SR;
	union {
		volatile uint32_t CNT;
		volatile uint32_t CNTL;
	};
	uint8_t RESERVED[4];
	union {
		volatile uint32_t CMP;
		volatile uint32_t CMPL;
	};
	uint8_t RESERVED0[4];
} SysTick_Type;

#define __I  volatile const  /*!< defines 'read only' permissions     */
#define __O  volatile        /*!< defines 'write only' permissions     */
#define __IO volatile        /*!< defines 'read / write' permissions   */
/* memory mapped structure for Program Fast Interrupt Controller (PFIC) */
typedef struct
{
	__I uint32_t  ISR[8];           // 0
	__I uint32_t  IPR[8];           // 20H
	__IO uint32_t ITHRESDR;         // 40H
	uint8_t       RESERVED[8];      // 44H
	__I uint32_t  GISR;             // 4CH
	__IO uint8_t  VTFIDR[4];        // 50H
	uint8_t       RESERVED0[0x0C];  // 54H
	__IO uint32_t VTFADDR[4];       // 60H
	uint8_t       RESERVED1[0x90];  // 70H
	__O uint32_t  IENR[8];          // 100H
	uint8_t       RESERVED2[0x60];  // 120H
	__O uint32_t  IRER[8];          // 180H
	uint8_t       RESERVED3[0x60];  // 1A0H
	__O uint32_t  IPSR[8];          // 200H
	uint8_t       RESERVED4[0x60];  // 220H
	__O uint32_t  IPRR[8];          // 280H
	uint8_t       RESERVED5[0x60];  // 2A0H
	__IO uint32_t IACTR[8];         // 300H
	uint8_t       RESERVED6[0xE0];  // 320H
	__IO uint8_t  IPRIOR[256];      // 400H
	uint8_t       RESERVED7[0x810]; // 500H
	__IO uint32_t SCTLR;            // D10H
} PFIC_Type;

#define CORE_PERIPH_BASE           (0xE0000000) /* System peripherals base address in the alias region */
#define PFIC_BASE                  (CORE_PERIPH_BASE + 0xE000)
#define PFIC                       ((PFIC_Type *) PFIC_BASE)
#define NVIC                       PFIC
#define NVIC_EnableIRQ(IRQn)       NVIC->IENR[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F))

#define SysTick_BASE               (CORE_PERIPH_BASE + 0xF000)
#define SysTick                    ((SysTick_Type *) SysTick_BASE)
#define SysTick_SR_SWIE            (1 << 31)
#define SysTick_SR_CNTIF           (1 << 0)
#define SysTick_LOAD_RELOAD_Msk    (0xFFFFFFFF)
#define SysTick_CTLR_MODE          (1 << 4)
#define SysTick_CTLR_STRE          (1 << 3)
#define SysTick_CTLR_STCLK         (1 << 2)
#define SysTick_CTLR_STIE          (1 << 1)
#define SysTick_CTLR_STE           (1 << 0)

__attribute__((section(".highcode_init")))
void highcode_init(void) {
	R32_MISC_CTRL |= (5 | (3<<25));
	R8_PLL_CONFIG &= ~(1<<5);
	R8_HFCK_PWR_CTRL |= RB_CLK_RC16M_PON | RB_CLK_PLL_PON;
	R16_CLK_SYS_CFG = (0x100 | 0x40 | 5); // CLK_SOURCE_HSI_PLL_62_4MHz;
	R8_FLASH_SCK &= ~(1<<4);
	R8_FLASH_CFG = 0x02;
	R8_XT32M_TUNE = (R8_XT32M_TUNE & (~0x03)) | 0x01;
	R8_CK32K_CONFIG |= RB_CLK_INT32K_PON;
}

void Clock78MHz() {
	SYS_SAFE_ACCESS(
		R8_HFCK_PWR_CTRL |= RB_CLK_PLL_PON;
		R8_FLASH_CFG = 0x01;
		R8_FLASH_SCK &= !(1<<4);
		R16_CLK_SYS_CFG = (0x300 | 0x40 | 4); // 78MHz
		// R16_CLK_SYS_CFG = (0x300 | 0x40 | 13); // 24MHz
	);
}

void DelayMs(int ms) {
	uint32_t targend = SysTick->CNTL + (ms * 78 * 1000); // 78MHz clock
	while( ((int32_t)( SysTick->CNTL - targend )) < 0 );
}

void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		GPIOA_ResetBits(GPIO_Pin_8);
		DelayMs(33);
		GPIOA_SetBits(GPIO_Pin_8);
		if(i) DelayMs(33);
	}
}

void char_debug(char c) {
	// this while is wasting clock ticks, but the easiest way to demo the debug interface
	// while(DMDATA0 & 0xc0);
	DMDATA0 = 0x85 | (c << 8);
}

void print(char msg[], int size, int endl) {
	for(int i = 0; i < size; i++) {
		char_debug(msg[i]);
	}
	if(endl) {
		char_debug('\r');
		char_debug('\n');
	}
}

void print_bytes(uint8_t data[], int size) {
	char hex_digits[] = "0123456789abcdef";
	char hx[] = "0x00 ";
	for(int i = 0; i < size; i++) {
		hx[2] = hex_digits[(data[i] >> 4) & 0x0F];
		hx[3] = hex_digits[data[i] & 0x0F];
		print(hx, 5, /*endl*/FALSE);
	}
	print(0, 0, /*endl*/TRUE);
}

#define MSG "~ ch585~"
int main(void) {
	Clock78MHz();
	GPIOA_ModeCfg_Out(GPIO_Pin_8);
	GPIOA_SetBits(GPIO_Pin_8);
	SysTick->CNTL = 0;
	SysTick->CMP = SysTick_LOAD_RELOAD_Msk -1;
	SysTick->CTLR = SysTick_CTLR_STRE  |
					SysTick_CTLR_STCLK |
					SysTick_CTLR_STIE  |
					SysTick_CTLR_STE; /* Enable SysTick IRQ and SysTick Timer */
	
	blink(5);
	print(MSG, sizeof(MSG), TRUE);

	while(1) {
		blink(1); // 33 ms
		DelayMs(SLEEPTIME_MS -33);
	}
}
