
ENTRY(_reset_vector)

MEMORY {
	
	/* 8k ROM is at the end of the memory */

	/* OTP lock is at 0xFFFFE000.*/
	/* OTP lock must be set to 0xFF or otherwise JTAG would be locked forever. */
	otp_lock (rw)			: ORIGIN = 0xFFFFE000, LENGTH = 0x4
	
	sysflash_rom_data (rw)	: ORIGIN = 0xFFFFE004, LENGTH = 0x200 - 0x4
	sysflash_rom (rw)		: ORIGIN = 0xFFFFE200, LENGTH = 8k - 0x4 - (0x200 - 0x4) - 0x4 - 16
	sysflash_rom_ver (r)	: ORIGIN = 0xFFFFFFEC, LENGTH = 0x4
	
	/* Reset vector is at 0xFFFFFFF0 just like for all other Intel cpus */
	reset_vector (r)		: ORIGIN = 0xFFFFFFF0, LENGTH = 16
	
	/* 80k of static RAM */
	
	sram_mem_idt (rw)		: ORIGIN = 0xA8000000, LENGTH = 8 * 68
	sram_mem (rw)			: ORIGIN = 0xA8000220, LENGTH = 80k - 1k - (8 * 68)
	sram_stack (rw)			: ORIGIN = 0xA8013C00, LENGTH = 1k
}

__idt_start = ORIGIN(sram_mem_idt);
__idt_end = __idt_start + LENGTH(sram_mem_idt);


SECTIONS {

	. = ORIGIN(reset_vector);
	
	.reset_vector : {
		*reset_vector.s.o (.text)
	} > reset_vector
	
	

}
