# How to rebuild the bootloader for BL602

Assuming you already sync the tree of [bl_iot_sdk](https://github.com/pine64/bl_iot_sdk)
to a local hard drive, follow the steps detailed in [official
document](https://pine64.github.io/bl602-docs/Quickstart_Guide/Linux/Quickstart_Linux_ubuntu.html).

The bootloader source code is inside the folder of bl_iot_sdk/customer_app/bl602_boot2. Here are
the steps to follow:

```bash
  cd bl_iot_sdk
  export BL60X_SDK_PATH=$(pwd)
  export CONFIG_CHIP_NAME=BL602
  cd customer_app/bl602_boot2
  make clean; make
```

This generate the images bl602_boot2.{bin, elf} inside build_out folder, along other artifacts.
If you take the bl602_boot2.bin and flash to the device along other pre-built binaries, you will
scratch the head: why this does not work? What happens? The main function in blsp_boot2.c is
like below:
```c
$ sed -n '380,420p' bl602_boot2/blsp_boot2.c

/****************************************************************************//**
 * @brief  Boot2 main function
 *
 * @param  None
 *
 * @return Return value
 *
*******************************************************************************/
int main(void)
{
    uint32_t ret=0,i=0;
    PtTable_Stuff_Config ptTableStuff[2];
    PtTable_ID_Type activeID;
    /* Init to zero incase only one cpu boot up*/
    PtTable_Entry_Config ptEntry[BFLB_BOOT2_CPU_MAX]={0};
    uint32_t bootHeaderAddr[BFLB_BOOT2_CPU_MAX]={0};
    uint8_t bootRollback[BFLB_BOOT2_CPU_MAX]={0};
    uint8_t ptParsed=1;
    uint8_t userFwName[9]={0};
#ifdef BLSP_BOOT2_ROLLBACK
    uint8_t rollBacked=0;
#endif
    uint8_t tempMode=0;
    Boot_Clk_Config clkCfg;
    uint8_t flashCfgBuf[4+sizeof(SPI_Flash_Cfg_Type)+4]={0};

    /* It's better not enable interrupt */
    //BLSP_Boot2_Init_Timer();

    /* Set RAM Max size */
    BLSP_Boot2_Disable_Other_Cache();

    /* Flush cache to get parameter */
    BLSP_Boot2_Flush_XIP_Cache();
    ret=BLSP_Boot2_Get_Clk_Cfg(&clkCfg);
    ret|=SF_Cfg_Get_Flash_Cfg_Need_Lock(0,&flashCfg);
    BLSP_Boot2_Flush_XIP_Cache();

    bflb_platform_print_set(BLSP_Boot2_Get_Log_Disable_Flag());
    bflb_platform_init(BLSP_Boot2_Get_Baudrate());
    ....
```

One of the most important artifacts is bl602_boot2.map, which gives us the clues of memory
layout usage. There is one line describing the main functions coming from bl602_mfg_flash.c
```
main /data/1208_bl_iot/bl_iot_sdk/customer_app/bl602_boot2/build_out/bl602_std/libbl602_std.a(bl602_mfg_flash.o)
```
The code is
```c
$ sed -n '25,28p' ./components/bl602/bl602_std/bl602_std/StdDriver/Src/bl602_mfg_flash.c
void main(void)
{

}
```
And the dump from elf confirms the incorrect main linked in the bootloader image.
```
$ riscv64-unknown-elf-objdump -D ./bl602_boot2.elf

230002aa <main>:
230002aa:   8082                    ret
230002ac:   0001                    nop
230002ae:   0101                    addi    sp,sp,0

```

Given the main function conflicts, the straightford fix is to rename the function *main* in
blsp_boot2.c, and explicitely call this specific *main* from \_enter in file entry.S. Here is
the fix
```c
diff --git a/components/bl602/bl602_std/bl602_std/RISCV/Device/Bouffalo/BL602/Startup/GCC/entry.S b/components/bl602/bl602_std/bl602_std/RISCV/Device/Bouffalo/BL602/Startup/GCC/entry.S
index dccee584..c44b2880 100644
--- a/components/bl602/bl602_std/bl602_std/RISCV/Device/Bouffalo/BL602/Startup/GCC/entry.S
+++ b/components/bl602/bl602_std/bl602_std/RISCV/Device/Bouffalo/BL602/Startup/GCC/entry.S
@@ -84,7 +84,7 @@ _enter:
     csrr a0, mhartid
     li a1, 0
     li a2, 0
-    call main
+    call bootloader_main

     /* If we've made it back here then there's probably something wrong.  We
      * allow the METAL to register a handler here. */
diff --git a/customer_app/bl602_boot2/bl602_boot2/blsp_boot2.c b/customer_app/bl602_boot2/bl602_boot2/blsp_boot2.c
index 2e705dbc..0706cd45 100644
--- a/customer_app/bl602_boot2/bl602_boot2/blsp_boot2.c
+++ b/customer_app/bl602_boot2/bl602_boot2/blsp_boot2.c
@@ -386,7 +386,7 @@ static void BLSP_Boot2_Get_MFG_StartReq(PtTable_ID_Type activeID,PtTable_Stuff_C
  * @return Return value
  *
 *******************************************************************************/
-int main(void)
+int bootloader_main(void)
 {
     uint32_t ret=0,i=0;
     PtTable_Stuff_Config ptTableStuff[2];
@@ -572,7 +572,7 @@ int main(void)

 void bfl_main()
 {
-    main();
+    bootloader_main();
 }

 /*@} end of group BLSP_BOOT2_Public_Functions */
```

You can confirm the correct function gets linked into the final image by checking the dump from
bl602_boot2.elf. And get a double confirmation from running experients on silicon board.
```
     1
     2 ./bl602_boot2.elf:     file format elf32-littleriscv
     3
     4
     5 Disassembly of section .text:
     6
     7 23000000 <__text_code_start__>:
     8 23000000:   1f022197            auipc   gp,0x1f022
     9 23000004:   88018193            addi    gp,gp,-1920 # 42021880 <__global_pointer$>
    10 23000008:   30047073            csrci   mstatus,8
    11 2300000c:   00005297            auipc   t0,0x5
    12 23000010:   af428293            addi    t0,t0,-1292 # 23004b00 <Trap_Handler_Stub>
    13 23000014:   0032e293            ori t0,t0,3
    14 23000018:   30529073            csrw    mtvec,t0
    15 2300001c:   00000293            li  t0,0
    16 23000020:   00028463            beqz    t0,23000028 <__text_code_start__+0x28>
    17 23000024:   7c105073            csrwi   0x7c1,0
    18 23000028:   1f020117            auipc   sp,0x1f020
    19 2300002c:   fd810113            addi    sp,sp,-40 # 42020000 <__StackTop>
    20 23000030:   00000297            auipc   t0,0x0
    21 23000034:   09028293            addi    t0,t0,144 # 230000c0 <__Vectors>
    22 23000038:   30729073            csrw    mtvt,t0
    23 2300003c:   301022f3            csrr    t0,misa
    24 23000040:   0202f293            andi    t0,t0,32
    25 23000044:   00028763            beqz    t0,23000052 <__text_code_start__+0x52>
    26 23000048:   6299                    lui t0,0x6
    27 2300004a:   3002a073            csrs    mstatus,t0
    28 2300004e:   00301073            fscsr   zero
    29 23000052:   731000ef            jal ra,23000f82 <SystemInit>
    30 23000056:   657000ef            jal ra,23000eac <start_load>
    31 2300005a:   f1402573            csrr    a0,mhartid
    32 2300005e:   4581                    li  a1,0
    33 23000060:   4601                    li  a2,0
    34 23000062:   4fa010ef            jal ra,2300155c <bootloader_main>
    35 23000066:   dd000097            auipc   ra,0xdd000
    36 2300006a:   f9a08093            addi    ra,ra,-102 # 0 <__metal_chicken_bit>
    37 2300006e:   00008363            beqz    ra,23000074 <__text_code_start__+0x74>
    38 23000072:   9082                    jalr    ra
    39 23000074:   00000297            auipc   t0,0x0
    40 23000078:   00c28293            addi    t0,t0,12 # 23000080 <__text_code_start__+0x80>
    41 2300007c:   30529073            csrw    mtvec,t0
    42 23000080:   00002303            lw  t1,0(zero) # 0 <__metal_chicken_bit>
    43 23000084:   bff5                    j   23000080 <__text_code_start__+0x80>
    44     ...
....
  1654 2300155c <bootloader_main>:
  1655 2300155c:   a3010113            addi    sp,sp,-1488
  1656 23001560:   04800613            li  a2,72
  1657 23001564:   4581                    li  a1,0
  1658 23001566:   00c8                    addi    a0,sp,68
  1659 23001568:   5c112623            sw  ra,1484(sp)
  1660 2300156c:   5c812423            sw  s0,1480(sp)
  1661 23001570:   5c912223            sw  s1,1476(sp)
  1662 23001574:   5d212023            sw  s2,1472(sp)
  1663 23001578:   5b312e23            sw  s3,1468(sp)
  1664 2300157c:   5b412c23            sw  s4,1464(sp)
  1665 23001580:   5b512a23            sw  s5,1460(sp)
  1666 23001584:   5b612823            sw  s6,1456(sp)
  1667 23001588:   5b712623            sw  s7,1452(sp)
  1668 2300158c:   5b812423            sw  s8,1448(sp)
  1669 23001590:   5b912223            sw  s9,1444(sp)
  1670 23001594:   5ba12023            sw  s10,1440(sp)
  1671 23001598:   59b12e23            sw  s11,1436(sp)
  1672 2300159c:   fa8ff0ef            jal ra,23000d44 <memset>
  1673 230015a0:   05c00613            li  a2,92
  1674 230015a4:   4581                    li  a1,0
  1675 230015a6:   0168                    addi    a0,sp,140
  1676 230015a8:   c602                    sw  zero,12(sp)
  1677 230015aa:   c802                    sw  zero,16(sp)
  1678 230015ac:   00011423            sh  zero,8(sp)
  1679 230015b0:   ca02                    sw  zero,20(sp)
  1680 230015b2:   cc02                    sw  zero,24(sp)
  1681 230015b4:   00010e23            sb  zero,28(sp)
  1682 230015b8:   f8cff0ef            jal ra,23000d44 <memset>
  1683 230015bc:   3ed000ef            jal ra,230021a8 <BLSP_Boot2_Disable_Other_Cache>
  1684 230015c0:   3eb000ef            jal ra,230021aa <BLSP_Boot2_Flush_XIP_Cache>
  1685 230015c4:   1008                    addi    a0,sp,32
  1686 230015c6:   ff00f097            auipc   ra,0xff00f
  1687 230015ca:   0b0080e7            jalr    176(ra) # 22010676 <BLSP_Boot2_Get_Clk_Cfg>
  1688 230015ce:   edc18593            addi    a1,gp,-292 # 4202175c <flashCfg>
....
```

