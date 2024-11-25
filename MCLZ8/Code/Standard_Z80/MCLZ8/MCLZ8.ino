//
//  Zilog Z80 emulator running on a Teensy 4.1.
//
//------------------------------------------------------------------------
//
// Copyright (c) 2022 Ted Fried
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------
//
// Copyright (c) 2024 Robert Atkinson
//
//------------------------------------------------------------------------

// Need for Command-line Code
#include <stdio.h>

#include <array>
#include <type_traits>
#include <stdint.h>

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef u32         BOOL;   // a not-converted-to-one-byte boolean

#if __GNUC__
#define INLINE __attribute__((always_inline)) inline
#else
#define INLINE inline
#endif


// Teensy 4.1 pin assignments
//
enum class PIN {
    RESET           = 23, // In-Buffer    1/8
    CLK             = 21, // In-Buffer    2/8   
    NMI             = 19, // In-Buffer    3/8   
    INTR            = 20, // In-Buffer    4/8
    WAIT            = 22, // In-Buffer    5/8        
         
    M1              = 12, // Out-direct        
    HALT            = 24, // Out-direct         
    RD              = 25, // Out-direct         
    WR              = 26, // Out-direct                                                     
    MREQ            = 27, // Out-direct                                                     
    IOREQ           = 28, // Out-direct        
    RFSH            = 29, // Out-direct 

    ADDR15          = 30, // Out-direct     
    ADDR14          = 31, // Out-direct     
    ADDR13          = 32, // Out-direct     
    ADDR12          = 33, // Out-direct     
    ADDR11          = 34, // Out-direct     
    ADDR10          = 35, // Out-direct     
    ADDR9           = 36, // Out-direct     
    ADDR8           = 37, // Out-direct     
                                                   
    AD7_OUT         = 10, // Out-Buffer        
    AD6_OUT         =  9, // Out-Buffer        
    AD5_OUT         =  8, // Out-Buffer        
    AD4_OUT         =  7, // Out-Buffer        
    AD3_OUT         =  6, // Out-Buffer        
    AD2_OUT         =  5, // Out-Buffer        
    AD1_OUT         =  4, // Out-Buffer        
    AD0_OUT         =  3, // Out-Buffer        
    DATA_OE_n       =  2, // Out-Buffer         
    ADDR_LATCH_n    = 11, // Out-direct     
                                                   
    AD7_IN          = 39, // In-Buffer        
    AD6_IN          = 40, // In-Buffer        
    AD5_IN          = 41, // In-Buffer        
    AD4_IN          = 14, // In-Buffer        
    AD3_IN          = 15, // In-Buffer        
    AD2_IN          = 16, // In-Buffer        
    AD1_IN          = 17, // In-Buffer        
    AD0_IN          = 18, // In-Buffer     
};

INLINE void PinMode(PIN pin, u8 mode) {
    pinMode(static_cast<u8>(pin), mode);
}

INLINE void DigitalWriteFast(PIN pin, u8 value) {
    digitalWriteFast(static_cast<u8>(pin), value);
}

   

// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------

#define OPCODE_READ_M1             0x07   
#define MEM_WRITE_BYTE             0x02    
#define MEM_READ_BYTE              0x03    
#define IO_WRITE_BYTE              0x04    
#define IO_READ_BYTE               0x05    
#define INTERRUPT_ACK              0x09


#define flag_s (  (register_f & 0x80)==0 ? 0 : 1  )
#define flag_z (  (register_f & 0x40)==0 ? 0 : 1  )
#define flag_h (  (register_f & 0x10)==0 ? 0 : 1  )
#define flag_p (  (register_f & 0x04)==0 ? 0 : 1  )
#define flag_n (  (register_f & 0x02)==0 ? 0 : 1  )
#define flag_c (  (register_f & 0x01)==0 ? 0 : 1  )


#define REGISTER_BC  ( (register_b<<8)   | register_c )
#define REGISTER_DE  ( (register_d<<8)   | register_e )
#define REGISTER_HL  ( (register_h<<8)   | register_l )
#define REGISTER_AF  ( (register_a<<8)   | register_f )
#define REGISTER_IX  ( (register_ixh<<8) | register_ixl )
#define REGISTER_IY  ( (register_iyh<<8) | register_iyl )

#define REG_BC 1
#define REG_DE 2
#define REG_HL 3
#define REG_AF 4
#define REG_IX 5
#define REG_IY 6

  
// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------
 

uint8_t  clock_counter=0;
uint8_t  pause_interrupts=0;
uint8_t  temp8=0;
uint8_t  opcode_byte=0;
uint8_t  opcode_second_byte=0;
uint8_t  debounce_refresh=0;
uint8_t  last_instruction_set_a_prefix=0;
uint8_t  and_opcode=0;
uint8_t  prefix_dd=0;
uint8_t  prefix_fd=0;   
uint8_t  prefix_cb=0;   
uint8_t  inc_dec=0;
uint8_t  with_carry=0;
uint8_t  CB_opcode=0;
uint8_t  cp_opcode=0;
uint8_t  halt_in_progress=0;
uint8_t  assert_iack_type0=0;
uint8_t  cb_prefix_offset=0;
uint8_t  register_a    = 0;         
uint8_t  register_b    = 0;
uint8_t  register_c    = 0;
uint8_t  register_d    = 0;
uint8_t  register_e    = 0;
uint8_t  register_f    = 0;                                     
uint8_t  register_h    = 0;
uint8_t  register_l    = 0;
uint8_t  register_a2   = 0;                                      
uint8_t  register_b2   = 0;
uint8_t  register_c2   = 0;
uint8_t  register_d2   = 0;
uint8_t  register_e2   = 0;
uint8_t  register_f2   = 0;                                     
uint8_t  register_h2   = 0;
uint8_t  register_l2   = 0;
uint8_t  register_iff1 = 0;
uint8_t  register_iff2 = 0;
uint8_t  register_im   = 0;
uint8_t  register_i    = 0;
uint8_t  register_r    = 0;
uint8_t  register_ixh  = 0;
uint8_t  register_ixl  = 0;
uint8_t  register_iyh  = 0;
uint8_t  register_iyl  = 0;
uint8_t  special  = 0;
uint8_t  mode=0;
int      incomingByte;   


uint16_t register_sp   = 0;                   
uint16_t register_pc   = 0;                   
uint16_t temp16=0;

// -------------------------------------------------
// GPIO and things related thereto
// -------------------------------------------------         

struct GPIO {
    static constexpr u32 GpioBit(int bitnum) {
        return static_cast<u32>(1) << bitnum;
    }
};

constexpr u8 Gpio6Data16(u16 raw) {
    constexpr u32 MASK_AD0_IN = GPIO::GpioBit(17);
    constexpr u32 MASK_AD1_IN = GPIO::GpioBit(22);
    constexpr u32 MASK_AD2_IN = GPIO::GpioBit(23);
    constexpr u32 MASK_AD3_IN = GPIO::GpioBit(19);
    constexpr u32 MASK_AD4_IN = GPIO::GpioBit(18);
    constexpr u32 MASK_AD5_IN = GPIO::GpioBit(21);
    constexpr u32 MASK_AD6_IN = GPIO::GpioBit(20);
    constexpr u32 MASK_AD7_IN = GPIO::GpioBit(29);

    constexpr u16 AD0 = (MASK_AD0_IN >> 16) & 0xFFFF;
    constexpr u16 AD1 = (MASK_AD1_IN >> 16) & 0xFFFF;
    constexpr u16 AD2 = (MASK_AD2_IN >> 16) & 0xFFFF;
    constexpr u16 AD3 = (MASK_AD3_IN >> 16) & 0xFFFF;
    constexpr u16 AD4 = (MASK_AD4_IN >> 16) & 0xFFFF;
    constexpr u16 AD5 = (MASK_AD5_IN >> 16) & 0xFFFF;
    constexpr u16 AD6 = (MASK_AD6_IN >> 16) & 0xFFFF;
    constexpr u16 AD7 = (MASK_AD7_IN >> 16) & 0xFFFF;

    u8 result = 0;
    result |= (raw & AD0) ? (1<<0) : 0;
    result |= (raw & AD1) ? (1<<1) : 0;
    result |= (raw & AD2) ? (1<<2) : 0;
    result |= (raw & AD3) ? (1<<3) : 0;
    result |= (raw & AD4) ? (1<<4) : 0;
    result |= (raw & AD5) ? (1<<5) : 0;
    result |= (raw & AD6) ? (1<<6) : 0;
    result |= (raw & AD7) ? (1<<7) : 0;
    return result;
}

constexpr auto GenGpio6Map() {
    std::array<u8, 65536> result = { 0 };
    for (u32 i = 0; i < 65536; ++i) {
        result[i] = Gpio6Data16(static_cast<u16>(i));
    }
    return result;
}

struct GPIO6 : GPIO {
private:
    static constexpr u32 MASK_RESET = GpioBit(25);
    static constexpr u32 MASK_WAIT  = GpioBit(24);
    static constexpr u32 MASK_CLK   = GpioBit(27);
    static constexpr u32 MASK_INTR  = GpioBit(26);
    static constexpr u32 MASK_NMI   = GpioBit(16);

    // A few tests
    static_assert(MASK_CLK  ==0x08000000);
    static_assert(MASK_WAIT ==0x01000000);
    static_assert(MASK_NMI  ==0x00010000);
    static_assert(MASK_RESET==0x02000000);

    // Last value we actually read
    u32 _raw = 0;

    INLINE BOOL Reset() const { return Reset(_raw); }
    INLINE BOOL Wait() const  { return Wait(_raw); }
    INLINE BOOL Intr() const  { return Intr(_raw); }
    INLINE BOOL Nmi()  const  { return Nmi(_raw); }

    static INLINE constexpr BOOL Reset(u32 raw) { return raw & MASK_RESET; }
    static INLINE constexpr BOOL Wait(u32 raw)  { return raw & MASK_WAIT; }
    static INLINE constexpr BOOL Intr(u32 raw)  { return raw & MASK_INTR; }
    static INLINE constexpr BOOL Nmi(u32 raw)   { return raw & MASK_NMI; }

    //-----------------------------------------------------------
    // Building fast map to extract data bytes from GPIO read

    static constexpr auto _dataMap = GenGpio6Map();

    // some tests
    static_assert(_dataMap[0] == 0x00);
    static_assert(_dataMap[1] == 0x00);
    static_assert(_dataMap[2] == 0x01);
    static_assert(_dataMap[3] == 0x01);
    static_assert(_dataMap[4] == 0x10);
    static_assert(_dataMap[5] == 0x10);
    static_assert(_dataMap[6] == 0x11);
    static_assert(_dataMap[7] == 0x11);
    static_assert(_dataMap[8] == 0x08);
    static_assert(_dataMap[9] == 0x08);
    static_assert(_dataMap[10] == 0x09);
    static_assert(_dataMap[11] == 0x09);
    static_assert(_dataMap[12] == 0x18);
    static_assert(_dataMap[13] == 0x18);
    static_assert(_dataMap[14] == 0x19);
    static_assert(_dataMap[15] == 0x19);
    static_assert(_dataMap[65506] == 0xa7);
    static_assert(_dataMap[65507] == 0xa7);
    static_assert(_dataMap[65508] == 0xb6);
    static_assert(_dataMap[65509] == 0xb6);
    static_assert(_dataMap[65510] == 0xb7);
    static_assert(_dataMap[65511] == 0xb7);
    static_assert(_dataMap[65512] == 0xae);
    static_assert(_dataMap[65513] == 0xae);
    static_assert(_dataMap[65514] == 0xaf);
    static_assert(_dataMap[65515] == 0xaf);
    static_assert(_dataMap[65516] == 0xbe);
    static_assert(_dataMap[65517] == 0xbe);
    static_assert(_dataMap[65518] == 0xbf);
    static_assert(_dataMap[65519] == 0xbf);
    static_assert(_dataMap[65520] == 0xe6);
    static_assert(_dataMap[65521] == 0xe6);
    static_assert(_dataMap[65522] == 0xe7);
    static_assert(_dataMap[65523] == 0xe7);
    static_assert(_dataMap[65524] == 0xf6);
    static_assert(_dataMap[65525] == 0xf6);
    static_assert(_dataMap[65526] == 0xf7);
    static_assert(_dataMap[65527] == 0xf7);
    static_assert(_dataMap[65528] == 0xee);
    static_assert(_dataMap[65529] == 0xee);
    static_assert(_dataMap[65530] == 0xef);
    static_assert(_dataMap[65531] == 0xef);
    static_assert(_dataMap[65532] == 0xfe);
    static_assert(_dataMap[65533] == 0xfe);
    static_assert(_dataMap[65534] == 0xff);
    static_assert(_dataMap[65535] == 0xff);

public:
    INLINE void Read() {
        _raw = GPIO6_DR;
    }

    INLINE BOOL Clk()  const  { return _raw & MASK_CLK; }

    INLINE BOOL ResetAsserted() const { return !Reset(); }
    INLINE BOOL WaitAsserted()  const { return !Wait();  }
    INLINE BOOL IntrAsserted()  const { return !Intr();  }
    INLINE BOOL NmiAsserted()   const { return !Nmi();   }

    INLINE u32 Raw() const { return _raw; }

    INLINE u8 Data() const {
        return Data(_raw);
    }

    static INLINE constexpr u16 High(u32 raw) {
        return static_cast<u16>(raw >> 16);
    }
    static INLINE constexpr u8 Data(u32 raw) {
        return _dataMap[High(raw)];
    }
};

GPIO6 gpio6;


// -------------------------------------------------
// Detecting interrupts
// -------------------------------------------------

struct NMI {
private:
    BOOL _asserted = false;
    BOOL _assertedPrev = false;
    bool _latched = false;   // latched on both clock edges

public:
    INLINE void Latch() {
        _asserted = gpio6.NmiAsserted();
        if (!_assertedPrev && _asserted) {
            _latched = true;
        }
        _assertedPrev = _asserted;
    }

    INLINE bool Latched() const {
        return _latched;
    }

    INLINE void Clear() {
        _asserted = false;
        _assertedPrev = false;
        ClearLatch();
    }
    INLINE void ClearLatch() {
        _latched = false;
    }
};

//--------------------

struct INTR {
private:
    BOOL _latched = false; // latched on rising edge

public:
    INLINE void Latch(BOOL latched) {
        _latched = latched;
    }

    // Have we detected an NMI?
    INLINE BOOL Latched() const {
        return _latched;
    }

    INLINE void Clear() {
        _latched = false;
    }
};

NMI nmi;
INTR intr;

// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------



// Internal RAM for acceleration
uint8_t   internal_RAM[65536];



// Arrays which hold the Z80 clock cycles for each opcode
constexpr uint8_t Opcode_Timing_Main[256] = {0x0,0x6,0x3,0x2,0x0,0x0,0x3,0x0,0x0,0x7,0x3,0x2,0x0,0x0,0x3,0x0,0x4,0x6,0x3,0x2,0x0,0x0,0x3,0x0,0x8,0x7,0x3,0x2,0x0,0x0,0x3,0x0,0x3,0x6,0xc,0x2,0x0,0x0,0x3,0x0,0x3,0x7,0xc,0x2,0x0,0x0,0x3,0x0,0x3,0x6,0x9,0x2,0x7,0x7,0x6,0x0,0x3,0x7,0x9,0x2,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x3,0x3,0x3,0x3,0x3,0x3,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x1,0x6,0x6,0x6,0x6,0x7,0x3,0x7,0x1,0x6,0x6,0x0,0x6,0xd,0x3,0x7,0x1,0x6,0x6,0x7,0x6,0x7,0x3,0x7,0x1,0x0,0x6,0x7,0x6,0x0,0x3,0x7,0x1,0x6,0x6,0xf,0x6,0x7,0x3,0x7,0x1,0x0,0x6,0x0,0x6,0x0,0x3,0x7,0x1,0x6,0x6,0x0,0x6,0x7,0x3,0x7,0x1,0x2,0x6,0x0,0x6,0x0,0x3,0x7 };

constexpr uint8_t Opcode_Timing_ED[256]   = {0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0x5,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0x5,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0x5,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0x5,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0xe,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0xe,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0x4,0x8,0x8,0xb,0x10,0x4,0xa,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0xc,0xc,0xc,0xc,0x4,0x4,0x4,0x4,0xc,0xc,0xc,0xc,0x4,0x4,0x4,0x4,0xc,0xc,0xc,0xc,0x4,0x4,0x4,0x4,0xc,0xc,0xc,0xc,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4 };

constexpr uint8_t Opcode_Timing_DDFD[256] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xb,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xb,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xa,0x10,0x6,0x4,0x4,0x7,0x0,0x0,0xb,0x10,0x6,0x4,0x4,0x7,0x0,0x0,0x0,0x0,0x0,0x13,0x13,0xf,0x0,0x0,0xb,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x4,0x4,0x4,0x4,0x4,0x4,0xf,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0xf,0x4,0xf,0xf,0xf,0xf,0xf,0xf,0x0,0xf,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x4,0x4,0xf,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xa,0x0,0x13,0x0,0xb,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x6,0x0,0x0,0x0,0x0,0x0,0x0 };

    
// Pre-calculated 8-bit parity
constexpr uint8_t Parity_Array[256] = {4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4 };


// Teensy 4.1 GPIO register mapping for address and data busses
constexpr uint32_t gpio7_low_array[0x100]  = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x400,0x400,0x400,0x400,0x400,0x400,0x400,0x400,0x20000,0x20000,0x20000,0x20000,0x20000,0x20000,0x20000,0x20000,0x20400,0x20400,0x20400,0x20400,0x20400,0x20400,0x20400,0x20400,0x10000,0x10000,0x10000,0x10000,0x10000,0x10000,0x10000,0x10000,0x10400,0x10400,0x10400,0x10400,0x10400,0x10400,0x10400,0x10400,0x30000,0x30000,0x30000,0x30000,0x30000,0x30000,0x30000,0x30000,0x30400,0x30400,0x30400,0x30400,0x30400,0x30400,0x30400,0x30400,0x800,0x800,0x800,0x800,0x800,0x800,0x800,0x800,0xc00,0xc00,0xc00,0xc00,0xc00,0xc00,0xc00,0xc00,0x20800,0x20800,0x20800,0x20800,0x20800,0x20800,0x20800,0x20800,0x20c00,0x20c00,0x20c00,0x20c00,0x20c00,0x20c00,0x20c00,0x20c00,0x10800,0x10800,0x10800,0x10800,0x10800,0x10800,0x10800,0x10800,0x10c00,0x10c00,0x10c00,0x10c00,0x10c00,0x10c00,0x10c00,0x10c00,0x30800,0x30800,0x30800,0x30800,0x30800,0x30800,0x30800,0x30800,0x30c00,0x30c00,0x30c00,0x30c00,0x30c00,0x30c00,0x30c00,0x30c00,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x401,0x401,0x401,0x401,0x401,0x401,0x401,0x401,0x20001,0x20001,0x20001,0x20001,0x20001,0x20001,0x20001,0x20001,0x20401,0x20401,0x20401,0x20401,0x20401,0x20401,0x20401,0x20401,0x10001,0x10001,0x10001,0x10001,0x10001,0x10001,0x10001,0x10001,0x10401,0x10401,0x10401,0x10401,0x10401,0x10401,0x10401,0x10401,0x30001,0x30001,0x30001,0x30001,0x30001,0x30001,0x30001,0x30001,0x30401,0x30401,0x30401,0x30401,0x30401,0x30401,0x30401,0x30401,0x801,0x801,0x801,0x801,0x801,0x801,0x801,0x801,0xc01,0xc01,0xc01,0xc01,0xc01,0xc01,0xc01,0xc01,0x20801,0x20801,0x20801,0x20801,0x20801,0x20801,0x20801,0x20801,0x20c01,0x20c01,0x20c01,0x20c01,0x20c01,0x20c01,0x20c01,0x20c01,0x10801,0x10801,0x10801,0x10801,0x10801,0x10801,0x10801,0x10801,0x10c01,0x10c01,0x10c01,0x10c01,0x10c01,0x10c01,0x10c01,0x10c01,0x30801,0x30801,0x30801,0x30801,0x30801,0x30801,0x30801,0x30801,0x30c01,0x30c01,0x30c01,0x30c01,0x30c01,0x30c01,0x30c01,0x30c01 };

constexpr uint32_t gpio9_low_array[0x100]  = {0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160,0x0,0x20,0x40,0x60,0x100,0x120,0x140,0x160 };

constexpr uint32_t gpio7_high_array[0x100] = {0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x0,0x80000,0x40000,0xc0000,0x10000000,0x10080000,0x10040000,0x100c0000,0x20000000,0x20080000,0x20040000,0x200c0000,0x30000000,0x30080000,0x30040000,0x300c0000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000,0x1000,0x81000,0x41000,0xc1000,0x10001000,0x10081000,0x10041000,0x100c1000,0x20001000,0x20081000,0x20041000,0x200c1000,0x30001000,0x30081000,0x30041000,0x300c1000 };

constexpr uint32_t gpio8_high_array[0x100] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x400000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0x800000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000,0xc00000 };

constexpr uint32_t gpio9_high_array[0x100] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80 };


// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------
//
// Begin Z80 Bus Interface Unit 
//
// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------

// Setup Teensy 4.1 IO's
//
void setup() {
    
  PinMode(PIN::RESET,        INPUT);  
  PinMode(PIN::CLK,          INPUT);  
  PinMode(PIN::NMI,          INPUT);  
  PinMode(PIN::INTR,         INPUT);  
  PinMode(PIN::WAIT,         INPUT);  
  
  PinMode(PIN::M1,           OUTPUT); 
  PinMode(PIN::HALT,         OUTPUT); 
  PinMode(PIN::RD,           OUTPUT); 
  PinMode(PIN::WR,           OUTPUT); 
  PinMode(PIN::MREQ,         OUTPUT); 
  PinMode(PIN::IOREQ,        OUTPUT); 
  PinMode(PIN::RFSH,         OUTPUT); 

  PinMode(PIN::ADDR15,       OUTPUT); 
  PinMode(PIN::ADDR14,       OUTPUT); 
  PinMode(PIN::ADDR13,       OUTPUT); 
  PinMode(PIN::ADDR12,       OUTPUT); 
  PinMode(PIN::ADDR11,       OUTPUT); 
  PinMode(PIN::ADDR10,       OUTPUT); 
  PinMode(PIN::ADDR9,        OUTPUT); 
  PinMode(PIN::ADDR8,        OUTPUT); 

  PinMode(PIN::AD7_OUT,      OUTPUT); 
  PinMode(PIN::AD6_OUT,      OUTPUT); 
  PinMode(PIN::AD5_OUT,      OUTPUT); 
  PinMode(PIN::AD4_OUT,      OUTPUT); 
  PinMode(PIN::AD3_OUT,      OUTPUT); 
  PinMode(PIN::AD2_OUT,      OUTPUT); 
  PinMode(PIN::AD1_OUT,      OUTPUT); 
  PinMode(PIN::AD0_OUT,      OUTPUT); 
  PinMode(PIN::DATA_OE_n,    OUTPUT); 
  PinMode(PIN::ADDR_LATCH_n, OUTPUT); 

  PinMode(PIN::AD7_IN,       INPUT); 
  PinMode(PIN::AD6_IN,       INPUT); 
  PinMode(PIN::AD5_IN,       INPUT); 
  PinMode(PIN::AD4_IN,       INPUT); 
  PinMode(PIN::AD3_IN,       INPUT); 
  PinMode(PIN::AD2_IN,       INPUT); 
  PinMode(PIN::AD1_IN,       INPUT); 
  PinMode(PIN::AD0_IN,       INPUT); 

  DigitalWriteFast(PIN::M1,0x1);
  DigitalWriteFast(PIN::HALT,0x1);
  DigitalWriteFast(PIN::RD,0x1);
  DigitalWriteFast(PIN::WR,0x1);
  DigitalWriteFast(PIN::MREQ,0x1);
  DigitalWriteFast(PIN::IOREQ,0x1);
  DigitalWriteFast(PIN::RFSH,0x1);
  DigitalWriteFast(PIN::ADDR_LATCH_n,0x1);
  
  Serial.begin(9600);
}
  

// ----------------------------------------------------------
// Address range check
//  Return: 0x0 - All external memory accesses
//          0x1 - Reads and writes are cycle accurate using internal memory with writes passing through to motherboard
//          0x2 - Reads accelerated using internal memory and writes are cycle accurate and pass through to motherboard
//          0x3 - All read and write accesses use accelerated internal memory 
// ----------------------------------------------------------
inline uint8_t internal_address_check(uint16_t local_address) {
    // if ( (local_address >= 0x0000) && (local_address <= 0x37DF) ) return mode;            //   TRS-80 Model III - ROM A, B, C   
    // if ( (local_address >= 0x3C00) && (local_address <= 0x3FFF) ) return 0x0;             //   TRS-80 Model III - 1KB Video RAM  
    // if ( (local_address >= 0x4000) && (local_address <= 0xFFFF) ) return mode;            //   TRS-80 Model III - 48KB DRAM
    return 0x0;
} 


// -------------------------------------------------
// Wait for the CLK rising edge  
// -------------------------------------------------         
inline void wait_for_CLK_rising_edge() {

    // First ensure clock is at a low level
    do {
        gpio6.Read();
    } while (gpio6.Clk());

    // Then poll for the first instance where clock is not low
    do {
        gpio6.Read();
    } while (!gpio6.Clk());

    if (debounce_refresh==1) {
        DigitalWriteFast(PIN::RFSH,0x1);
        debounce_refresh=0;
    }

    intr.Latch(gpio6.IntrAsserted());
    nmi.Latch();
    
    return;
}
    
// -------------------------------------------------
// Wait for the CLK falling edge. Side effect: latch GPIO6
// -------------------------------------------------
inline void wait_for_CLK_falling_edge() {

    // Count down clock_counter here at end of each instruction
    if (clock_counter>0) {
        clock_counter--;
    }

    // First ensure clock is at a high level
    do {
        gpio6.Read();
    } while (!gpio6.Clk());

    // Then poll for the first instance where clock is not high
    do {
        gpio6.Read();  
    } while (gpio6.Clk());

    // Store slightly-delayed version of GPIO6 in a global register
    gpio6.Read();

    nmi.Latch();
    
    return;
}
  
// -------------------------------------------------
// Initiate a Z80 Bus Cycle
// -------------------------------------------------
inline uint8_t BIU_Bus_Cycle(uint8_t biu_operation, uint16_t local_address, uint8_t local_data)  {

    // For non-IO cycles, check address for the acceleration mode
    //
    u8 local_mode;
    if  ((biu_operation!=IO_WRITE_BYTE) && (biu_operation!=IO_READ_BYTE) ) {
        local_mode = internal_address_check(local_address);
    } else {
        local_mode=0;
    }

    u8 read_cycle = (biu_operation & 0x1);

    u8 m1_cycle;
    if ( (biu_operation==OPCODE_READ_M1) || ( biu_operation==INTERRUPT_ACK) ) {
        m1_cycle=0;
    } else {
        m1_cycle=2;
    }
    
    
    if (local_mode>1 && ((biu_operation==OPCODE_READ_M1)||(biu_operation==MEM_READ_BYTE)) ) {
        return internal_RAM[local_address];                     // Mode 2, 3 Reads
    }
    if (local_mode>2 && local_address>=0x4000 && biu_operation==MEM_WRITE_BYTE ) {
        internal_RAM[local_address] = local_data;  return 0xEE; // Mode 3 Writes
    }       

    noInterrupts();                                  // Disable Teensy interrupts so the Z80 bus cycle can complete without interruption
    DigitalWriteFast(PIN::ADDR_LATCH_n,0x1);   // Allow address to propagate through '373 latch
    
  
    // Pre-calculate before the clock edge to gain speed
    //
    u8  local_address_high = (local_address)>>8;
    u8  local_address_low  = (0x00FF&local_address);
    u32 gpio7_out = gpio7_high_array[local_address_high] | gpio7_low_array[local_address_low];
    u32 gpio8_out = gpio8_high_array[local_address_high] ;
    u32 gpio9_out = gpio9_high_array[local_address_high] | gpio9_low_array[local_address_low];
    
    u32 writeback_data7 = (0xCFF0E3FC & GPIO7_DR) | m1_cycle;   
    u32 writeback_data9 = (0xFFFFFE1F & GPIO9_DR);   
    u32 writeback_data8 = (0xFF3FFFFF & GPIO8_DR);   


    // Address phase is common for all bus cycle types - Lower byte of address is multiplexed with data so needs to be externally latched
    // -----------------------------------------------------------------------------------------------------------------------------------------
    wait_for_CLK_rising_edge();
    GPIO7_DR = writeback_data7 | gpio7_out; // T1 Rising 
    GPIO8_DR = writeback_data8 | gpio8_out;
    GPIO9_DR = writeback_data9 | gpio9_out | 0x80000010; // disable PIN_RFSH and PIN_DATA_OE_n
	if ( (biu_operation==OPCODE_READ_M1) || (biu_operation==INTERRUPT_ACK) )  {
	    DigitalWriteFast(PIN::M1,0x0);
    }


    // Opcode fetch - M1 type - 4 cycles
    // -----------------------------------------------------------------------------
    if (biu_operation == OPCODE_READ_M1)  {

        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::MREQ,0x0);                                    // T1 Falling
        DigitalWriteFast(PIN::RD,0x0);
        //DigitalWriteFast(PIN::ADDR_LATCH_n,0x0);

        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();                                              // T2 Falling
        while (gpio6.WaitAsserted()) {
            wait_for_CLK_falling_edge();
        }               

        // Pre-calculate before the next clock edge to gain speed
        //
        local_address_high = register_i;
        local_address_low  = register_r;
        gpio7_out = gpio7_high_array[local_address_high] | gpio7_low_array[local_address_low];
        gpio8_out = gpio8_high_array[local_address_high] ;
        gpio9_out = gpio9_high_array[local_address_high] | gpio9_low_array[local_address_low];
          
        writeback_data7 = (0xCFF0E3FC & GPIO7_DR) | 0x2; // drive M1 high again   
        writeback_data8 = (0xFF3FFFFF & GPIO8_DR);    
        writeback_data9 = (0xFFFFFE1F & GPIO9_DR);    

        //-------------------------------------------------------------------------------------
        wait_for_CLK_rising_edge();
        u32 read_data_raw = gpio6.Raw();                                           // T3 Rising   -- Read data sampled on this edge
                                        
        // Drive Refresh address
        //
        GPIO9_DR        = writeback_data9 | gpio9_out;                          
        GPIO8_DR        = writeback_data8 | gpio8_out;
        GPIO7_DR        = writeback_data7 | gpio7_out; 

        DigitalWriteFast(PIN::MREQ,0x1); 
        DigitalWriteFast(PIN::RFSH,0x0);  
        DigitalWriteFast(PIN::RD,0x1); 
        DigitalWriteFast(PIN::M1,0x1);  

        //-------------------------------------------------------------------------------------                              
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::MREQ,0x0);                                     // T3 Falling
        u8 read_data = gpio6.Data(read_data_raw);                                  // Read the Z80 bus data, re-arranging bits

        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::MREQ,0x1);                                     // T4 Falling
        debounce_refresh=1;

        //-------------------------------------------------------------------------------------
        interrupts(); 
        
        if (local_mode>0)  {
            read_data = internal_RAM[local_address]; // Mode 1 Reads
        }
        
        return read_data;
    }

        
    // Memory Read/Write - 3 cycles
    // -----------------------------------------------------------------------------    
    else if ( (biu_operation==MEM_READ_BYTE) || (biu_operation==MEM_WRITE_BYTE) )  {
        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::ADDR_LATCH_n,0x0);                             // T1 Falling
        DigitalWriteFast(PIN::MREQ,0x0);
        DigitalWriteFast(PIN::RD,!read_cycle);

        DigitalWriteFast(PIN::AD7_OUT,  (local_data & 0x80) );         
        DigitalWriteFast(PIN::AD6_OUT,  (local_data & 0x40) );
        DigitalWriteFast(PIN::AD5_OUT,  (local_data & 0x20) );
        DigitalWriteFast(PIN::AD4_OUT,  (local_data & 0x10) );
        DigitalWriteFast(PIN::AD3_OUT,  (local_data & 0x08) );
        DigitalWriteFast(PIN::AD2_OUT,  (local_data & 0x04) );
        DigitalWriteFast(PIN::AD1_OUT,  (local_data & 0x02) );
        DigitalWriteFast(PIN::AD0_OUT,  (local_data & 0x01) );     
        DigitalWriteFast(PIN::DATA_OE_n, read_cycle);

        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::WR,read_cycle);                                      // T2 Falling
        while (gpio6.WaitAsserted()) {
            wait_for_CLK_falling_edge();
        }                     

        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::MREQ,0x1);                                     // T3 Falling   -- Read data sampled on this edge
        DigitalWriteFast(PIN::RD,0x1); 
        DigitalWriteFast(PIN::WR,0x1); 
        u8 read_data = gpio6.Data();                                               // Read the Z80 bus data, re-arranging bits
                              
        interrupts();   
        
        if ( (local_address>=0x4000) && read_cycle==0)  internal_RAM[local_address] = local_data;               // Mode 1 Writes -- Always write to internal RAM
        if (local_mode>0)                   read_data = internal_RAM[local_address];                            // Mode 1 Reads

        return read_data;
    }

        
    // IO Read/Write - 3 cycles with one additional wait state
    // -----------------------------------------------------------------------------  
    else if ( (biu_operation==IO_READ_BYTE) || (biu_operation==IO_WRITE_BYTE) )  {
        
        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::ADDR_LATCH_n,0x0);                            // T1 Falling
        DigitalWriteFast(PIN::AD7_OUT,  (local_data & 0x80) );                   
        DigitalWriteFast(PIN::AD6_OUT,  (local_data & 0x40) );
        DigitalWriteFast(PIN::AD5_OUT,  (local_data & 0x20) );
        DigitalWriteFast(PIN::AD4_OUT,  (local_data & 0x10) );
        DigitalWriteFast(PIN::AD3_OUT,  (local_data & 0x08) );
        DigitalWriteFast(PIN::AD2_OUT,  (local_data & 0x04) );
        DigitalWriteFast(PIN::AD1_OUT,  (local_data & 0x02) );
        DigitalWriteFast(PIN::AD0_OUT,  (local_data & 0x01) );        
        DigitalWriteFast(PIN::DATA_OE_n, read_cycle);
                          
                          
        //-------------------------------------------------------------------------------------
        wait_for_CLK_rising_edge();
        DigitalWriteFast(PIN::IOREQ,0x0);                                  // T2 Rising
        DigitalWriteFast(PIN::RD,!read_cycle);
        DigitalWriteFast(PIN::WR,read_cycle);
                                      

        //-------------------------------------------------------------------------------------
        wait_for_CLK_rising_edge();                                                                             // Tw Rising        
        wait_for_CLK_rising_edge();                                                                             // T3 Rising        
        wait_for_CLK_falling_edge();
        while (gpio6.WaitAsserted()) {
            wait_for_CLK_falling_edge();                                         // T3 Falling        -- Read data sampled on this edge  
        }               
        DigitalWriteFast(PIN::IOREQ,0x1);                                       
        DigitalWriteFast(PIN::RD,0x1); 
        DigitalWriteFast(PIN::WR,0x1); 
        u8 read_data = gpio6.Data();                                             // Read the Z80 bus data, re-arranging bits

        interrupts(); 
        return read_data;
    }

    
    // Interrupt Acknowledge - 4 cycle M1 type but with two additional wait states
    // -----------------------------------------------------------------------------  
    else if (biu_operation==INTERRUPT_ACK)  {

        //-------------------------------------------------------------------------------------
        wait_for_CLK_rising_edge();                                                                             // T2 Rising
        wait_for_CLK_rising_edge();                                                                             // Tw1 Rising
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::IOREQ,0x0);                                  // Tw1 Falling

        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        while (gpio6.WaitAsserted()) {
            wait_for_CLK_falling_edge();                                         // Tw2 Falling
        }              

        // Pre-calculate before the next clock edge to gain speed
        //
        local_address_high = register_i;
        local_address_low  = register_r;
        gpio7_out = gpio7_high_array[local_address_high] | gpio7_low_array[local_address_low];
        gpio8_out = gpio8_high_array[local_address_high] | 0x00040000 ; // Also set IOREQ_n high
        gpio9_out = gpio9_high_array[local_address_high] | gpio9_low_array[local_address_low];

        writeback_data7 = (0xCFF0E3FC & GPIO7_DR) | 0x2; // drive M1 high again   
        writeback_data9 = (0xFFFFFE1F & GPIO9_DR);   // Read in current GPIOx register value and clear the bits we intend to update
        writeback_data8 = (0xFF3FFFFF & GPIO8_DR);   // Read in current GPIOx register value and clear the bits we intend to update
          

        //-------------------------------------------------------------------------------------
        wait_for_CLK_rising_edge();
        u32 read_data_raw = gpio6.Raw();                                          // T3 Rising   -- Read data sampled on this edge
                                      
        // Drive Refresh address
        //
        GPIO9_DR        = writeback_data9 | gpio9_out;                          
        GPIO8_DR        = writeback_data8 | gpio8_out;
        GPIO7_DR        = writeback_data7 | gpio7_out; 

        DigitalWriteFast(PIN::M1,0x1); 
        DigitalWriteFast(PIN::RFSH,0x0);  
        DigitalWriteFast(PIN::IOREQ,0x1);

                                          
        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::MREQ,0x0);                                    // T3 Falling
        u8 read_data = gpio6.Data(read_data_raw);                                 // Read the Z80 bus data, re-arranging bits

                                      
        //-------------------------------------------------------------------------------------
        wait_for_CLK_falling_edge();
        DigitalWriteFast(PIN::MREQ,0x1);                                    // T4 Falling
        debounce_refresh=1;

        interrupts(); 
        return read_data;
    }   


return 0xEE;
}


// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------
//
// End Z80 Bus Interface Unit
//
// --------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------


uint16_t Sign_Extend(uint8_t local_data)  {  if ((0x0080&local_data)!=0) return (0xFF00|local_data); else return (0x00FF&local_data); }


// ------------------------------------------------------
// Z80 Reset routine
// ------------------------------------------------------
//
void reset_sequence()  {

    // Stay here until RESET is de-asserted
    while (gpio6.ResetAsserted()) {
        wait_for_CLK_falling_edge();
    }

    clock_counter=0;                                                 // Debounce prefix, cycle counter, and nmi
    last_instruction_set_a_prefix=0;   
    prefix_dd = 0;
    prefix_fd = 0;
    prefix_cb = 0;  
    pause_interrupts=0;     
    halt_in_progress=0;
    nmi.Clear();
    intr.Clear();
    
    register_iff1       = 0;                                        // Reset registers
    register_iff2       = 0;                                     
    register_i          = 0;                                     
    register_r          = 0;                                     
    register_im         = 0;                                     
    register_pc         = 0;                                     
    register_sp         = 0xFFFF;                                  
    register_a          = 0x00;                                  
    register_f          = 0xFF;                                  
    
    wait_for_CLK_rising_edge();
    return;
}


// ------------------------------------------------------
// Fetch an opcode byte - M1 cycle
// ------------------------------------------------------

uint8_t Fetch_opcode()  {    
    uint8_t local_byte;
    
    if (assert_iack_type0==1)  {
        local_byte = BIU_Bus_Cycle(INTERRUPT_ACK, 0x0000, 0x00); 
    } else {
        local_byte = BIU_Bus_Cycle(OPCODE_READ_M1, register_pc, 0x00);
    }
    
    assert_iack_type0=0;
    register_pc++;
    register_r = (register_r&0x80) | (0x7F&(register_r+1));

    return local_byte;
}

// ------------------------------------------------------
// Fetch a subsequent byte 
// ------------------------------------------------------

uint8_t Fetch_byte()  {
    uint8_t local_byte = BIU_Bus_Cycle(MEM_READ_BYTE, register_pc, 0x00);
    register_pc++;
    return local_byte;
}


// ------------------------------------------------------
// Read and Write data from BIU
// ------------------------------------------------------

uint8_t Read_byte(uint16_t local_address)  {
    uint8_t local_byte = BIU_Bus_Cycle(MEM_READ_BYTE, local_address, 0x00);
    return local_byte;
}

uint16_t Read_word(uint16_t local_address)  {
    uint8_t local_byte1 = BIU_Bus_Cycle(MEM_READ_BYTE, local_address, 0x00);
    uint8_t local_byte2 = BIU_Bus_Cycle(MEM_READ_BYTE, local_address + 1, 0x00);
    return (local_byte1 | (local_byte2<<8));
}

void Write_byte(uint16_t local_address , uint8_t local_data)  {    
    BIU_Bus_Cycle(MEM_WRITE_BYTE , local_address , local_data );
}

void Write_word(uint16_t local_address , uint16_t local_data)  {    
    BIU_Bus_Cycle(MEM_WRITE_BYTE , local_address   , local_data    );  
    BIU_Bus_Cycle(MEM_WRITE_BYTE , local_address+1 , local_data>>8 );  
}

void Writeback_Reg16(uint8_t local_reg , uint16_t local_data)  {
    switch (local_reg)  {
        case REG_BC: register_b=(local_data>>8);    register_c=(0xFF&local_data);    break; 
        case REG_DE: register_d=(local_data>>8);    register_e=(0xFF&local_data);    break; 
        case REG_HL: register_h=(local_data>>8);    register_l=(0xFF&local_data);    break; 
        case REG_AF: register_a=(local_data>>8);    register_f=(0xFF&local_data);    break; 
        case REG_IX: register_ixh=(local_data>>8);  register_ixl=(0xFF&local_data);  break; 
        case REG_IY: register_iyh=(local_data>>8);  register_iyl=(0xFF&local_data);  break;
    }
}


// ------------------------------------------------------
// Prefix Opcodes
// ------------------------------------------------------
void opcode_0xDD () {
    prefix_dd=1;
    last_instruction_set_a_prefix=1;
    clock_counter = clock_counter + Opcode_Timing_DDFD[opcode_byte];
}

void opcode_0xFD () {
    prefix_fd=1;
    last_instruction_set_a_prefix=1;
    clock_counter = clock_counter + Opcode_Timing_DDFD[opcode_byte];
}

void opcode_0x00 () { // NOP
    return;
}


// ------------------------------------------------------
// Stack
// ------------------------------------------------------

void Push(uint16_t local_data)  {    
    register_sp--;
    BIU_Bus_Cycle(MEM_WRITE_BYTE , register_sp   , local_data>>8    );  // High Byte
    register_sp--;
    BIU_Bus_Cycle(MEM_WRITE_BYTE , register_sp , local_data );          // Low Byte
}

uint16_t Pop()  {
    u8 local_data_low = BIU_Bus_Cycle(MEM_READ_BYTE, register_sp, 0x00); // Low Byte
    register_sp++;
    u8 local_data_high = BIU_Bus_Cycle(MEM_READ_BYTE, register_sp, 0x00); // High Byte
    register_sp++;
    return( (local_data_high<<8) | local_data_low);
}

void opcode_0xC5()  {  Push(REGISTER_BC);  return;  }                                                                                                   // push bc
void opcode_0xD5()  {  Push(REGISTER_DE);  return;  }                                                                                                   // push de
void opcode_0xE5()  {  if (prefix_dd==1)  Push(REGISTER_IX); else if (prefix_fd==1) Push(REGISTER_IY); else Push(REGISTER_HL);  return;  }              // push hl
void opcode_0xF5()  {  Push(REGISTER_AF);  return;  }                                                                                                   // push af

void opcode_0xF1()  {  uint16_t local_data = Pop();                            register_a  =(local_data>>8); register_f  =(local_data&0xFF);  return;  } // pop af
void opcode_0xC1()  {  uint16_t local_data = Pop();                            register_b  =(local_data>>8); register_c  =(local_data&0xFF);  return;  } // pop bc
void opcode_0xD1()  {  uint16_t local_data = Pop();                            register_d  =(local_data>>8); register_e  =(local_data&0xFF);  return;  } // pop de
void opcode_0xE1()  {  uint16_t local_data = Pop();        if (prefix_dd==1) { register_ixh=(local_data>>8); register_ixl=(local_data&0xFF);}            // pop ix
                                                      else if (prefix_fd==1) { register_iyh=(local_data>>8); register_iyl=(local_data&0xFF);}            // pop iy
                                                      else                   { register_h  =(local_data>>8); register_l  =(local_data&0xFF);} return;  } // pop hl
                                                       

    
// ------------------------------------------------------
// Flags/Complements 
// ------------------------------------------------------

void opcode_0x3F()  {   temp8 = register_f & 0x01;                          // Store old C Flag     
                        register_f = register_f & 0xC4;                     // Clear H, C, N, 5,3
                        if (temp8==1) register_f = register_f | 0x10;       // Set H
                        if (temp8==0) register_f = register_f | 0x01;       // Complement C Flag
                        register_f = register_f | (register_a&0x28); 
                        return;  }  // ccf


void opcode_0x2F()  {   register_a = 0xFF ^ register_a;                     // Complement register a    
                        register_f = register_f | 0x12;                     // Set N and H Flags
                        register_f = register_f & 0xD7;                     // Clear Flags 5,3
                        register_f = register_f | (register_a&0x28); 
                        return;  }  // cpl


void opcode_0x37()  {   register_f = register_f | 0x01;                     // Set C
                        register_f = register_f & 0xC5;                     // Clear Flags H, N, 5,3
                        register_f = register_f | (register_a&0x28); 
                        return;  }  // scf



// ------------------------------------------------------
// Interrupt enables and modes 
// ------------------------------------------------------
void opcode_0xED46()  {  register_im=0;                                                                                   return;  }  // IM0
void opcode_0xED56()  {  register_im=1;                                                                                   return;  }  // IM1
void opcode_0xED5E()  {  register_im=2;                                                                                   return;  }  // IM2
void opcode_0xFB()  { register_iff1=1; register_iff2=1;   last_instruction_set_a_prefix=0;                                return;  }  // ei
void opcode_0xF3()  { register_iff1=0; register_iff2=0;                                                                   return;  }  // di


// ------------------------------------------------------
// Exchanges 
// ------------------------------------------------------
void opcode_0x08()  { temp8=register_a; register_a=register_a2; register_a2=temp8;    temp8=register_f; register_f=register_f2; register_f2=temp8;   return;  }   // ex af,af' 
void opcode_0xEB()  { temp8=register_d; register_d=register_h;  register_h=temp8;     temp8=register_e; register_e=register_l;  register_l=temp8;    return;  }   // ex de hl 

void opcode_0xD9()  { temp8=register_b; register_b=register_b2; register_b2=temp8;    temp8=register_c; register_c=register_c2; register_c2=temp8;                // exx  bc  bc' 
                      temp8=register_d; register_d=register_d2; register_d2=temp8;    temp8=register_e; register_e=register_e2; register_e2=temp8;                //      de  de' 
                      temp8=register_h; register_h=register_h2; register_h2=temp8;    temp8=register_l; register_l=register_l2; register_l2=temp8;   return;  }   //      hl  hl'

void opcode_0xE3()  {  if (prefix_dd==1) { temp8=Read_byte(register_sp);    Write_byte(register_sp  ,register_ixl);  register_ixl=temp8;                          // ex (sp),ix                                                                           
                                           temp8=Read_byte(register_sp+1);  Write_byte(register_sp+1,register_ixh);  register_ixh=temp8;  }  else                                                                                      
                       if (prefix_fd==1) { temp8=Read_byte(register_sp);    Write_byte(register_sp  ,register_iyl);  register_iyl=temp8;                          // ex (sp),iy                                                                           
                                           temp8=Read_byte(register_sp+1);  Write_byte(register_sp+1,register_iyh);  register_iyh=temp8;  }  else                                                                                      

                                         { temp8=Read_byte(register_sp);    Write_byte(register_sp  ,register_l);    register_l=temp8;                            // ex (sp),hl                                                                           
                                           temp8=Read_byte(register_sp+1);  Write_byte(register_sp+1,register_h);    register_h=temp8;    }  return;  }                                                                                      


// ------------------------------------------------------
// Jumps, Calls, Returns
// ------------------------------------------------------

void Jump_Not_Taken8()  {
    Fetch_byte();
    return;
}
void Jump_Not_Taken16()  {
    Fetch_byte();
    Fetch_byte();
    return;
}
void Jump_Taken8()  {
    uint16_t local_displacement = Sign_Extend(Fetch_byte());
    register_pc               = (register_pc) + local_displacement;
    return;
}
void Jump_Taken16()  {
    uint16_t local_displacement = Fetch_byte();                                        
    register_pc               = (Fetch_byte()<<8) | local_displacement; 
    return;
}
void opcode_0x18()  {                                            clock_counter=clock_counter+5; Jump_Taken8();                                              return;  }  // jr *    - Disp8
void opcode_0x20()  { if (flag_z == 0)                          {clock_counter=clock_counter+5; Jump_Taken8();}    else Jump_Not_Taken8();                  return;  }  // jr nz,* - Disp8
void opcode_0x28()  { if (flag_z == 1)                          {clock_counter=clock_counter+5; Jump_Taken8();}    else Jump_Not_Taken8();                  return;  }  // jr z,*  - Disp8
void opcode_0x30()  { if (flag_c == 0)                          {clock_counter=clock_counter+5; Jump_Taken8();}    else Jump_Not_Taken8();                  return;  }  // jr nc,* - Disp8
void opcode_0x38()  { if (flag_c == 1)                          {clock_counter=clock_counter+5; Jump_Taken8();}    else Jump_Not_Taken8();                  return;  }  // jr c,*  - Disp8
void opcode_0x10()  { register_b--;      if (register_b != 0)   {clock_counter=clock_counter+5; Jump_Taken8();}    else Jump_Not_Taken8();                  return;  }  // djnz *  - Disp8
            
void opcode_0xC2()  { if (flag_z == 0)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp nz,**  
void opcode_0xCA()  { if (flag_z == 1)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp z,**  
void opcode_0xD2()  { if (flag_c == 0)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp nc,**  
void opcode_0xDA()  { if (flag_c == 1)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp c,**  
void opcode_0xE2()  { if (flag_p == 0)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp po,**  
void opcode_0xEA()  { if (flag_p == 1)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp pe,**  
void opcode_0xF2()  { if (flag_s == 0)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp p,**  
void opcode_0xFA()  { if (flag_s == 1)                           Jump_Taken16();  else Jump_Not_Taken16();                                                  return;  }  // jp m,**  

void opcode_0xC3()  {                                            Jump_Taken16();                                                                            return;  }  // jp ** 
void opcode_0xE9()  { if (prefix_dd==1) register_pc = (REGISTER_IX); else if (prefix_fd==1) register_pc = (REGISTER_IY); else register_pc = (REGISTER_HL);  return;  }  // jp ix or iy, or (hl) 

void opcode_0xC4()  { if (flag_z == 0) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call nz,** 
void opcode_0xCC()  { if (flag_z == 1) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call z,** 
void opcode_0xD4()  { if (flag_c == 0) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call nc,** 
void opcode_0xDC()  { if (flag_c == 1) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call c,** 
void opcode_0xE4()  { if (flag_p == 0) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call po,** 
void opcode_0xEC()  { if (flag_p == 1) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call pe,** 
void opcode_0xF4()  { if (flag_s == 0) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call p,** 
void opcode_0xFC()  { if (flag_s == 1) {    clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();   }  else Jump_Not_Taken16();               return;  }  // call m,** 
void opcode_0xCD()  {                       clock_counter=clock_counter+7; Push(register_pc+2); Jump_Taken16();                                             return;  }  // call ** 

void opcode_0xC9()  {                       clock_counter=clock_counter+6; register_pc = Pop();         return;  }  // ret 
void opcode_0xC0()  { if (flag_z == 0)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret nz 
void opcode_0xC8()  { if (flag_z == 1)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret z 
void opcode_0xD0()  { if (flag_c == 0)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret nc 
void opcode_0xD8()  { if (flag_c == 1)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret c 
void opcode_0xE0()  { if (flag_p == 0)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret po 
void opcode_0xE8()  { if (flag_p == 1)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret pe 
void opcode_0xF0()  { if (flag_s == 0)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret p 
void opcode_0xF8()  { if (flag_s == 1)     {clock_counter=clock_counter+6; register_pc = Pop(); }       return;  }  // ret m 
void opcode_0xED45()  { register_iff1=register_iff2;   register_pc = Pop();                             return;  }  // retn
void opcode_0xED4D()  {                     register_pc = Pop();                                        return;  }  // reti 
                        
void opcode_0xC7()  {    Push(register_pc); register_pc = 0x00;                                         return;  }  // rst 00h
void opcode_0xD7()  {    Push(register_pc); register_pc = 0x10;                                         return;  }  // rst 10h
void opcode_0xE7()  {    Push(register_pc); register_pc = 0x20;                                         return;  }  // rst 20h
void opcode_0xF7()  {    Push(register_pc); register_pc = 0x30;                                         return;  }  // rst 30h
void opcode_0xCF()  {    Push(register_pc); register_pc = 0x08;                                         return;  }  // rst 08h
void opcode_0xDF()  {    Push(register_pc); register_pc = 0x18;                                         return;  }  // rst 18h
void opcode_0xEF()  {    Push(register_pc); register_pc = 0x28;                                         return;  }  // rst 28h
void opcode_0xFF()  {    Push(register_pc); register_pc = 0x38;                                         return;  }  // rst 38h


// ------------------------------------------------------
// Loads
// ------------------------------------------------------
void opcode_0x01 () {  register_c = Fetch_byte();  register_b = Fetch_byte();       return;  }
void opcode_0x11 () {  register_e = Fetch_byte();  register_d = Fetch_byte();       return;  }
void opcode_0x0A () {  register_a = Read_byte(REGISTER_BC);                             return;  }
void opcode_0x0E () {  register_c = Fetch_byte();                                     return;  }
void opcode_0x1E () {  register_e = Fetch_byte();                                     return;  }
void opcode_0x1A () {  register_a = Read_byte(REGISTER_DE);                             return;  }  

void opcode_0x21 () {  if (prefix_dd==1) { register_ixl = Fetch_byte(); register_ixh = Fetch_byte();  }   else
                       if (prefix_fd==1) { register_iyl = Fetch_byte(); register_iyh = Fetch_byte();  }   else
                                         { register_l   = Fetch_byte(); register_h   = Fetch_byte();  }   return;  }
                    
void opcode_0x22 () {
    uint16_t local_address = Fetch_byte();
    local_address          = (Fetch_byte()<<8) | local_address;

    if (prefix_dd==1) { Write_byte(local_address , register_ixl); Write_byte( local_address+1 , register_ixh); }   else
    if (prefix_fd==1) { Write_byte(local_address , register_iyl); Write_byte( local_address+1 , register_iyh); }   else
                      { Write_byte(local_address , register_l);   Write_byte( local_address+1 , register_h);   }   
    return;  }
                            
void opcode_0x2A () {
    uint16_t local_address = Fetch_byte();
    local_address        = (Fetch_byte()<<8) | local_address; 
    
    if (prefix_dd==1) { register_ixl = Read_byte(local_address); register_ixh = Read_byte(local_address+1);  }   else
    if (prefix_fd==1) { register_iyl = Read_byte(local_address); register_iyh = Read_byte(local_address+1);  }   else
                      { register_l   = Read_byte(local_address); register_h   = Read_byte(local_address+1);  }   
    return;  }
    
void opcode_0x06 () { register_b = Fetch_byte();   return;  }
void opcode_0x16 () { register_d = Fetch_byte();   return;  }
void opcode_0x26 () { if (prefix_dd==1) { register_ixh = Fetch_byte(); }   else if (prefix_fd==1) { register_iyh = Fetch_byte(); }   else  register_h = Fetch_byte();   return;  }
void opcode_0x2E () { if (prefix_dd==1) { register_ixl = Fetch_byte(); }   else if (prefix_fd==1) { register_iyl = Fetch_byte(); }   else  register_l = Fetch_byte();   return;  }




void opcode_0x31 () {
    uint16_t local_address = Fetch_byte();
    local_address = (Fetch_byte()<<8) | local_address;

    register_sp = local_address;  return;  }
    
void opcode_0x32 () {
    uint16_t local_address = Fetch_byte();
    local_address = (Fetch_byte()<<8) | local_address;

    Write_byte(local_address , register_a);        
return;  }  

void opcode_0x36 () {  if (prefix_dd==1) Write_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) , Fetch_byte() );  else
                       if (prefix_fd==1) Write_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) , Fetch_byte() );  else
                                         Write_byte(REGISTER_HL                               , Fetch_byte() );  return;  }


    
void opcode_0x3A () {   register_a = Read_byte((Fetch_byte()|(Fetch_byte()<<8)));  return;  }
void opcode_0x3E () {   register_a = Fetch_byte();   return;  }
    
void opcode_0x40 () {  register_b = register_b;  return;  }
void opcode_0x41 () {  register_b = register_c;  return;  }
void opcode_0x42 () {  register_b = register_d;  return;  }
void opcode_0x43 () {  register_b = register_e;  return;  }
void opcode_0x44 () {  if (prefix_dd==1) register_b = register_ixh;  else if (prefix_fd==1) register_b = register_iyh;   else  register_b = register_h; return;  }
void opcode_0x45 () {  if (prefix_dd==1) register_b = register_ixl;  else if (prefix_fd==1) register_b = register_iyl;   else  register_b = register_l; return;  }
void opcode_0x46 () {  if (prefix_dd==1) register_b = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_b = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_b = Read_byte(REGISTER_HL);                           return;  }
void opcode_0x47 () {  register_b = register_a;  return;  }
void opcode_0x48 () {  register_c = register_b;  return;  }
void opcode_0x49 () {  register_c = register_c;  return;  }
void opcode_0x4A () {  register_c = register_d;  return;  }
void opcode_0x4B () {  register_c = register_e;  return;  }
void opcode_0x4C () {  if (prefix_dd==1) register_c = register_ixh;  else if (prefix_fd==1) register_c = register_iyh;   else  register_c = register_h; return;  }
void opcode_0x4D () {  if (prefix_dd==1) register_c = register_ixl;  else if (prefix_fd==1) register_c = register_iyl;   else  register_c = register_l; return;  }
void opcode_0x4E () {  if (prefix_dd==1) register_c = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_c = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_c = Read_byte(REGISTER_HL);                           return;  }
void opcode_0x4F () {  register_c = register_a;  return;  }

// ----------------------------------------

void opcode_0x50 () {  register_d = register_b;  return;  }
void opcode_0x51 () {  register_d = register_c;  return;  }
void opcode_0x52 () {  register_d = register_d;  return;  }
void opcode_0x53 () {  register_d = register_e;  return;  }
void opcode_0x54 () {  if (prefix_dd==1) register_d = register_ixh;  else if (prefix_fd==1) register_d = register_iyh;   else  register_d = register_h; return;  }
void opcode_0x55 () {  if (prefix_dd==1) register_d = register_ixl;  else if (prefix_fd==1) register_d = register_iyl;   else  register_d = register_l; return;  }
void opcode_0x56 () {  if (prefix_dd==1) register_d = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_d = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_d = Read_byte(REGISTER_HL);                           return;  }
void opcode_0x57 () {  register_d = register_a;  return;  }
void opcode_0x58 () {  register_e = register_b;  return;  }
void opcode_0x59 () {  register_e = register_c;  return;  }
void opcode_0x5A () {  register_e = register_d;  return;  }
void opcode_0x5B () {  register_e = register_e;  return;  }
void opcode_0x5C () {  if (prefix_dd==1) register_e = register_ixh;  else if (prefix_fd==1) register_e = register_iyh;   else  register_e = register_h; return;  }
void opcode_0x5D () {  if (prefix_dd==1) register_e = register_ixl;  else if (prefix_fd==1) register_e = register_iyl;   else  register_e = register_l; return;  }
void opcode_0x5E () {  if (prefix_dd==1) register_e = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_e = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_e = Read_byte(REGISTER_HL);                           return;  }
void opcode_0x5F () {  register_e = register_a;  return;  }

// ----------------------------------------

void opcode_0x60 () {  if (prefix_dd==1) register_ixh = register_b;    else if (prefix_fd==1) register_iyh = register_b;     else  register_h = register_b; return;  }
void opcode_0x61 () {  if (prefix_dd==1) register_ixh = register_c;    else if (prefix_fd==1) register_iyh = register_c;     else  register_h = register_c; return;  }
void opcode_0x62 () {  if (prefix_dd==1) register_ixh = register_d;    else if (prefix_fd==1) register_iyh = register_d;     else  register_h = register_d; return;  }
void opcode_0x63 () {  if (prefix_dd==1) register_ixh = register_e;    else if (prefix_fd==1) register_iyh = register_e;     else  register_h = register_e; return;  }
void opcode_0x64 () {  if (prefix_dd==1) register_ixh = register_ixh;  else if (prefix_fd==1) register_iyh = register_iyh;   else  register_h = register_h; return;  }
void opcode_0x65 () {  if (prefix_dd==1) register_ixh = register_ixl;  else if (prefix_fd==1) register_iyh = register_iyl;   else  register_h = register_l; return;  }
void opcode_0x66 () {  if (prefix_dd==1) register_h = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_h = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_h = Read_byte(REGISTER_HL);                           return;  }
void opcode_0x67 () {  if (prefix_dd==1) register_ixh = register_a;    else if (prefix_fd==1) register_iyh = register_a;     else  register_h = register_a; return;  }
void opcode_0x68 () {  if (prefix_dd==1) register_ixl = register_b;    else if (prefix_fd==1) register_iyl = register_b;     else  register_l = register_b; return;  }
void opcode_0x69 () {  if (prefix_dd==1) register_ixl = register_c;    else if (prefix_fd==1) register_iyl = register_c;     else  register_l = register_c; return;  }
void opcode_0x6A () {  if (prefix_dd==1) register_ixl = register_d;    else if (prefix_fd==1) register_iyl = register_d;     else  register_l = register_d; return;  }
void opcode_0x6B () {  if (prefix_dd==1) register_ixl = register_e;    else if (prefix_fd==1) register_iyl = register_e;     else  register_l = register_e; return;  }
void opcode_0x6C () {  if (prefix_dd==1) register_ixl = register_ixh;  else if (prefix_fd==1) register_iyl = register_iyh;   else  register_l = register_h; return;  }
void opcode_0x6D () {  if (prefix_dd==1) register_ixl = register_ixl;  else if (prefix_fd==1) register_iyl = register_iyl;   else  register_l = register_l; return;  }
void opcode_0x6E () {  if (prefix_dd==1) register_l = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_l = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_l = Read_byte(REGISTER_HL);                           return;  }
void opcode_0x6F () {  if (prefix_dd==1) register_ixl = register_a;    else if (prefix_fd==1) register_iyl = register_a;     else  register_l = register_a; return;  }

// ----------------------------------------

void opcode_0x02 () {  Write_byte(REGISTER_BC , register_a);   return;  }  // ld (bc),a
void opcode_0x12 () {  Write_byte(REGISTER_DE , register_a);   return;  }  // ld (de),a


void opcode_0x70 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_b);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_b);  else
                                         Write_byte( REGISTER_HL                               , register_b );    return;  }
                    
void opcode_0x71 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_c);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_c);  else
                                         Write_byte( REGISTER_HL                               , register_c );    return;  }

void opcode_0x72 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_d);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_d);  else
                                         Write_byte( REGISTER_HL                               , register_d );    return;  }
                    
void opcode_0x73 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_e);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_e);  else
                                         Write_byte( REGISTER_HL                               , register_e );    return;  }

void opcode_0x74 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_h);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_h);  else
                                         Write_byte( REGISTER_HL                               , register_h );    return;  }
                    
void opcode_0x75 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_l);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_l);  else
                                         Write_byte( REGISTER_HL                               , register_l );    return;  }
                    
void opcode_0x77 () {  if (prefix_dd==1) Write_byte( (REGISTER_IX+Sign_Extend(Fetch_byte())) , register_a);  else
                       if (prefix_fd==1) Write_byte( (REGISTER_IY+Sign_Extend(Fetch_byte())) , register_a);  else
                                         Write_byte( REGISTER_HL                               , register_a );    return;  }
void opcode_0x78 () {  register_a = register_b; return;  }
void opcode_0x79 () {  register_a = register_c; return;  }
void opcode_0x7A () {  register_a = register_d; return;  }
void opcode_0x7B () {  register_a = register_e; return;  }
void opcode_0x7C () {  if (prefix_dd==1) register_a = register_ixh;    else if (prefix_fd==1) register_a = register_iyh;     else  register_a = register_h; return;  }
void opcode_0x7D () {  if (prefix_dd==1) register_a = register_ixl;    else if (prefix_fd==1) register_a = register_iyl;     else  register_a = register_l; return;  }
void opcode_0x7E () {  if (prefix_dd==1) register_a = Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) );  else
                       if (prefix_fd==1) register_a = Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) );  else
                                         register_a = Read_byte(REGISTER_HL);    return;  }
void opcode_0x7F () {  register_a = register_a; return;  }

// ----------------------------------------


void opcode_0xF9 () {  if (prefix_dd==1) register_sp = ((register_ixh<<8) | register_ixl);   else   
                       if (prefix_fd==1) register_sp = ((register_iyh<<8) | register_iyl);   else   
                                         register_sp = ((register_h  <<8) | register_l  );   return;  }   


void opcode_0xED43 () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    Write_byte(local_address , register_c);   Write_byte( local_address+1 , register_b);      
    return;  }
            

void opcode_0xED53 () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    Write_byte(local_address , register_e);   Write_byte( local_address+1 , register_d);      
    return;  }
            

void opcode_0xED63 () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    Write_byte(local_address , register_l);   Write_byte( local_address+1 , register_h);      
    return;  }
            

void opcode_0xED73 () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    Write_word(local_address , register_sp);  
    return;  }
            

void opcode_0xED4B () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    register_c   = Read_byte(local_address);
    register_b   = Read_byte(local_address+1);
    return;  }


void opcode_0xED5B () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    register_e   = Read_byte(local_address);
    register_d   = Read_byte(local_address+1);
    return;  }

void opcode_0xED6B () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    register_l   = Read_byte(local_address);
    register_h   = Read_byte(local_address+1);
    return;  }

void opcode_0xED7B () {
    uint16_t local_address = (Fetch_byte() | (Fetch_byte() << 8));
    register_sp   = Read_word(local_address);
    return;  }
    
    
void opcode_0xED47 () {  register_i = register_a;  return;  }                   // ld i, a

void opcode_0xED57 () {  register_a = register_i;                               // ld a, i
                         register_f = register_f & 0x29;                        // Clear S, Z, H, P, N flags
                         if (register_iff2) register_f=register_f|0x04;         // Set P flag
                         register_f = register_f | (register_a&0x80);           // Set S flag
                         if (register_a==0)  register_f = register_f | 0x40;    // Set Z flag                        
                         return;  } 

void opcode_0xED4F () {  register_r = register_a;  return;  }

void opcode_0xED5F () {  register_a = register_r; 
                         register_f = register_f & 0x29;                        // Clear S, Z, H, P, N flags
                         if (register_iff2) register_f=register_f|0x04;         // Set P flag
                         register_f = register_f | (register_a&0x80);           // Set S flag
                         if (register_a==0)  register_f = register_f | 0x40;    // Set Z flag                        
                         return;  } 


// ------------------------------------------------------
// Boolean
// ------------------------------------------------------

void Flags_Boolean() {
    register_f = register_f & 0x00;                         // Clear S, Z, H, P, N, C flags
    if (and_opcode==1) register_f = register_f | 0x10;      // Set H flag
    register_f = register_f | Parity_Array[register_a];     // Set P flag
    register_f = register_f | (register_a&0x80);            // Set S flag
    if (register_a==0)  register_f = register_f | 0x40;     // Set Z flag
    register_f = register_f | (register_a&0x28);            // Set flag bits 5,3 to ALU results
    and_opcode=0;
    return;
}
    
    
void opcode_0xA0 () {  register_a=(register_a & register_b);      and_opcode=1; Flags_Boolean();  return;  }  // and b
void opcode_0xA1 () {  register_a=(register_a & register_c);      and_opcode=1; Flags_Boolean();  return;  }  // and c
void opcode_0xA2 () {  register_a=(register_a & register_d);      and_opcode=1; Flags_Boolean();  return;  }  // and d
void opcode_0xA3 () {  register_a=(register_a & register_e);      and_opcode=1; Flags_Boolean();  return;  }  // and e
void opcode_0xA4 () {  if (prefix_dd==1) register_a=(register_a & register_ixh);  else                        // and h , ixh, iyh            
                       if (prefix_fd==1) register_a=(register_a & register_iyh);  else              
                                         register_a=(register_a & register_h); 
                       and_opcode=1; Flags_Boolean();  return;  }  
void opcode_0xA5 () {  if (prefix_dd==1) register_a=(register_a & register_ixl);  else                        // and l , ixl, iyl            
                       if (prefix_fd==1) register_a=(register_a & register_iyl);  else              
                                         register_a=(register_a & register_l); 
                       and_opcode=1; Flags_Boolean();  return;  }  
void opcode_0xA6 () {  if (prefix_dd==1) register_a=(register_a & Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) ));  else      // and (hl) , ix+*, iy+*            
                       if (prefix_fd==1) register_a=(register_a & Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) ));  else             
                                         register_a=(register_a & Read_byte(REGISTER_HL)); 
                       and_opcode=1; Flags_Boolean();  return;  }                        
void opcode_0xA7 () {  register_a=(register_a & register_a);       and_opcode=1; Flags_Boolean();  return;  }  // and a
void opcode_0xE6 () {  register_a=(register_a & Fetch_byte());     and_opcode=1;        Flags_Boolean();  return;  }  // and *
        
// ----
        
void opcode_0xA8 () {  register_a=(register_a ^ register_b);                Flags_Boolean();  return;  }  // xor b
void opcode_0xA9 () {  register_a=(register_a ^ register_c);                Flags_Boolean();  return;  }  // xor c
void opcode_0xAA () {  register_a=(register_a ^ register_d);                Flags_Boolean();  return;  }  // xor d
void opcode_0xAB () {  register_a=(register_a ^ register_e);                Flags_Boolean();  return;  }  // xor e
void opcode_0xAC () {  if (prefix_dd==1) register_a=(register_a ^ register_ixh);  else                    // xor h , ixh, iyh            
                       if (prefix_fd==1) register_a=(register_a ^ register_iyh);  else              
                                         register_a=(register_a ^ register_h); 
                       Flags_Boolean();  return;  }  
void opcode_0xAD () {  if (prefix_dd==1) register_a=(register_a ^ register_ixl);  else                    // xor l , ixl, iyl            
                       if (prefix_fd==1) register_a=(register_a ^ register_iyl);  else              
                                         register_a=(register_a ^ register_l); 
                       Flags_Boolean();  return;  }  
void opcode_0xAE () {  if (prefix_dd==1) register_a=(register_a ^ Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) ));  else      // xor (hl) , ix+*, iy+*            
                       if (prefix_fd==1) register_a=(register_a ^ Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) ));  else             
                                         register_a=(register_a ^ Read_byte(REGISTER_HL)); 
                       Flags_Boolean();  return;  }  
void opcode_0xAF () {  register_a=(register_a ^ register_a);                Flags_Boolean();  return;  }  // xor a
void opcode_0xEE () {  register_a=(register_a ^ Fetch_byte());            Flags_Boolean();  return;  }  // xor *
        
// ----
    
void opcode_0xB0 () {  register_a=(register_a | register_b);                Flags_Boolean();  return;  }  // or b
void opcode_0xB1 () {  register_a=(register_a | register_c);                Flags_Boolean();  return;  }  // or c
void opcode_0xB2 () {  register_a=(register_a | register_d);                Flags_Boolean();  return;  }  // or d
void opcode_0xB3 () {  register_a=(register_a | register_e);                Flags_Boolean();  return;  }  // or e
void opcode_0xB4 () {  if (prefix_dd==1) register_a=(register_a | register_ixh);  else                    // or h , ixh, iyh            
                       if (prefix_fd==1) register_a=(register_a | register_iyh);  else              
                                         register_a=(register_a | register_h); 
                       Flags_Boolean();  return;  }  
void opcode_0xB5 () {  if (prefix_dd==1) register_a=(register_a | register_ixl);  else                    // or l , ixl, iyl            
                       if (prefix_fd==1) register_a=(register_a | register_iyl);  else              
                                         register_a=(register_a | register_l); 
                       Flags_Boolean();  return;  }  
void opcode_0xB6 () {  if (prefix_dd==1) register_a=(register_a | Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()) ));  else      // or (hl) , ix+*, iy+*            
                       if (prefix_fd==1) register_a=(register_a | Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()) ));  else             
                                         register_a=(register_a | Read_byte(REGISTER_HL)); 
                       Flags_Boolean();  return;  }  
void opcode_0xB7 () {  register_a=(register_a | register_a);                Flags_Boolean();  return;  }  // or a
void opcode_0xF6 () {  register_a=(register_a | Fetch_byte());            Flags_Boolean();  return;  }  // or *


// ------------------------------------------------------
// Addition for bytes
// ------------------------------------------------------
uint8_t ADD_Bytes(uint8_t local_data1 , uint8_t local_data2)  {
    uint8_t   local_nibble_results;
    uint16_t  local_byte_results;
    uint16_t  operand0=0;
    uint16_t  operand1=0;
    uint16_t  result=0;
    uint8_t   local_cf=0;  
    
    local_cf=(flag_c);
    
    register_f = register_f & 0x01;                                                                     // Clear S, Z, H, V, N flags

    if (with_carry==1) {  local_nibble_results = (0x0F&local_data1) + (0x0F&local_data2) + local_cf;    // Perform the nibble math
                          local_byte_results   = local_data1 + local_data2 + local_cf;                  // Perform the byte math 
    }
    else               {  local_nibble_results = (0x0F&local_data1) + (0x0F&local_data2);               // Perform the nibble math
                          local_byte_results   = local_data1 + local_data2;                             // Perform the byte math 
    }

    if (local_nibble_results > 0x0F)                 register_f = (register_f | 0x10);                  // Set H Flag
    if ( inc_dec==0 )  {  if (local_byte_results > 0xFF)  register_f = (register_f | 0x01); else register_f = (register_f & 0xFE);  }                  // Set C Flag if not INC or DEC opcodes
    inc_dec = 0;                                                                                        // Debounce inc_dec

    operand0 = (local_data1         & 0x0080);  
    operand1 = (local_data2         & 0x0080);  
    result   = (local_byte_results  & 0x0080);   
    if      (operand0==0 && operand1==0 && result!=0)  register_f = (register_f | 0x04);                // Set V Flag
    else if (operand0!=0 && operand1!=0 && result==0)  register_f = (register_f | 0x04);       
    
    register_f = register_f | (local_byte_results&0x80);                                                // Set S flag
    if ((0xFF&local_byte_results)==0)  register_f = register_f | 0x40;                                  // Set Z flag
    register_f = register_f | (local_byte_results&0x28);            // Set flag bits 5,3 to ALU results
    with_carry=0;
        
    return local_byte_results;
}

// ------------------------------------------------------
// Addition for words  
// ------------------------------------------------------
uint16_t ADD_Words(uint16_t local_data1 , uint16_t local_data2)  {
    uint16_t  local_nibble_results;
    uint32_t  local_word_results;
    uint8_t   local_cf=0;  
    
    local_cf=(flag_c);
    
    register_f = register_f & 0xC5;                                                                     // Clear H, N flags

    
    if (with_carry==1) {  local_nibble_results = (0x0FFF&local_data1) + (0x0FFF&local_data2) + local_cf;    // Perform the nibble math
                          local_word_results   = local_data1 + local_data2 + local_cf;                  // Perform the word math 
    }
    else               {  local_nibble_results = (0x0FFF&local_data1) + (0x0FFF&local_data2);               // Perform the nibble math
                          local_word_results   = local_data1 + local_data2;                             // Perform the word math 
    }


    if (local_nibble_results > 0x0FFF)                 register_f = (register_f | 0x10);                 // Set H Flag
    if ( inc_dec==0 )  {  if (local_word_results > 0xFFFF)  register_f = (register_f | 0x01); else register_f = (register_f & 0xFE);  }                  // Set C Flag if not INC or DEC opcodes
    register_f = register_f | ((local_word_results>>8)&0x28);            // Set flag bits 5,3 to ALU results
    inc_dec = 0;                                                                                        // Debounce inc_dec
    with_carry=0;

    return local_word_results;
}


// ------------------------------------------------------
// Addition for words2 - Sets all flags
// ------------------------------------------------------
uint16_t ADD_Words2(uint16_t local_data1 , uint16_t local_data2)  {
    uint16_t   local_nibble_results;
    uint32_t  local_word_results;
    uint16_t  operand0=0;
    uint16_t  operand1=0;
    uint16_t  result=0;
    uint8_t   local_cf=0;  
    
    local_cf=(flag_c);
    
    register_f = register_f & 0x01;                                                                     // Clear S, Z, H, V, N flags

    
    if (with_carry==1) {  local_nibble_results = (0x0FFF&local_data1) + (0x0FFF&local_data2) + local_cf;    // Perform the nibble math
                          local_word_results   = local_data1 + local_data2 + local_cf;                  // Perform the word math 
    }
    else               {  local_nibble_results = (0x0FFF&local_data1) + (0x0FFF&local_data2);               // Perform the nibble math
                          local_word_results   = local_data1 + local_data2;                             // Perform the word math 
    }


    if (local_nibble_results > 0x0FFF)                 register_f = (register_f | 0x10);                 // Set H Flag
    if ( inc_dec==0 )  {  if (local_word_results > 0xFFFF)  register_f = (register_f | 0x01); else register_f = (register_f & 0xFE);  }                  // Set C Flag if not INC or DEC opcodes
    inc_dec = 0;                                                                                         // Debounce inc_dec


    operand0 = (local_data1         & 0x8000);  
    operand1 = (local_data2         & 0x8000);  
    result   = (local_word_results  & 0x8000); 
    if      (operand0==0 && operand1==0 && result!=0)  register_f = (register_f | 0x04);              // Set V Flag
    else if (operand0!=0 && operand1!=0 && result==0)  register_f = (register_f | 0x04);       
    
    if (local_word_results&0x8000)register_f = register_f | 0x80;                                       // Set S flag
    if (local_word_results==0)    register_f = register_f | 0x40;                                       // Set Z flag
    register_f = register_f | ((local_word_results>>8)&0x28);            // Set flag bits 5,3 to ALU results
    with_carry=0;

    return local_word_results;
}


// ------------------------------------------------------
// Subtraction for bytes
// ------------------------------------------------------
uint8_t SUB_Bytes(uint8_t local_data1 , uint8_t local_data2)  {
    uint8_t   local_nibble_results;
    uint16_t  local_byte_results;
    uint16_t  operand0=0;
    uint16_t  operand1=0;
    uint16_t  result=0;
    uint8_t   local_cf=0;  
    
    local_cf=(flag_c);
    
    register_f = register_f & 0x01;                                                                     // Clear S, Z, H, V, N flags

    
    if (with_carry==1) {  local_nibble_results = (0x0F&local_data1) - (0x0F&local_data2) - local_cf;    // Perform the nibble math
                          local_byte_results   = local_data1 - local_data2 - local_cf;                  // Perform the byte math 
    }
    else               {  local_nibble_results = (0x0F&local_data1) - (0x0F&local_data2);               // Perform the nibble math
                          local_byte_results   = local_data1 - local_data2;                             // Perform the byte math 
    }

    if (local_nibble_results > 0x0F)                 register_f = (register_f | 0x10);                  // Set H Flag
    if ( inc_dec==0 )  {  if (local_byte_results > 0xFF)  register_f = (register_f | 0x01); else register_f = (register_f & 0xFE);  }                  // Set C Flag if not INC or DEC opcodes
    inc_dec = 0;                                                                                        // Debounce inc_dec


    operand0 = (local_data1         & 0x0080);  
    operand1 = (local_data2         & 0x0080);  
    result   = (local_byte_results  & 0x0080); 
    if      (operand0==0 && operand1!=0 && result!=0)  register_f = (register_f | 0x04);              // Set V Flag
    else if (operand0!=0 && operand1==0 && result==0)  register_f = (register_f | 0x04);       
    
    register_f = register_f | (local_byte_results&0x80);                                                // Set S flag
    if ((0xFF&local_byte_results)==0)  register_f = register_f | 0x40;                                  // Set Z flag
    register_f = register_f | 0x02;                                                                     // Set N flag always for subtraction  
    if (cp_opcode==0) register_f = register_f | (local_byte_results&0x28);   else          // Set flag bits 5,3 to ALU results
                      register_f = register_f | (local_data2&0x28);             // Set flag bits 5,3 to ALU results
    with_carry=0;
    cp_opcode=0;

    return local_byte_results;
}

// ------------------------------------------------------
// Subtraction for words
// ------------------------------------------------------
uint16_t SUB_Words(uint16_t local_data1 , uint16_t local_data2)  {
    uint16_t   local_nibble_results;
    uint32_t  local_word_results;
    uint16_t  operand0=0;
    uint16_t  operand1=0;
    uint16_t  result=0;
    uint8_t   local_cf=0;  
    
    local_cf=(flag_c);
    
    register_f = register_f & 0x01;                                                                         // Clear S, Z, H, V, N flags

    
    if (with_carry==1) {  local_nibble_results = (0x0FFF&local_data1) - (0x0FFF&local_data2) - local_cf;    // Perform the nibble math
                          local_word_results   = local_data1 - local_data2 - local_cf;                      // Perform the word math 
    }
    else               {  local_nibble_results = (0x0FFF&local_data1) - (0x0FFF&local_data2);               // Perform the nibble math
                          local_word_results   = local_data1 - local_data2;                                 // Perform the word math 
    }

    if (local_nibble_results > 0x0FFF)                   register_f = (register_f | 0x10);                  // Set H Flag
    if ( inc_dec==0 )  {  if (local_word_results > 0xFFFF)  register_f = (register_f | 0x01); else register_f = (register_f & 0xFE);  }                  // Set C Flag if not INC or DEC opcodes
    inc_dec = 0;                                                                                            // Debounce inc_dec


    operand0 = (local_data1         & 0x8000);  
    operand1 = (local_data2         & 0x8000);  
    result   = (local_word_results  & 0x8000); 
    if      (operand0==0 && operand1!=0 && result!=0)  register_f = (register_f | 0x04);              // Set V Flag
    else if (operand0!=0 && operand1==0 && result==0)  register_f = (register_f | 0x04);       

    if (local_word_results&0x8000)register_f = register_f | 0x80;                                       // Set S flag
    if (local_word_results==0)    register_f = register_f | 0x40;                                       // Set Z flag
    register_f = register_f | 0x02;                                                                     // Set N flag always for subtraction
    register_f = register_f | ((local_word_results>>8)&0x28);            // Set flag bits 5,3 to ALU results
    with_carry=0;

    return local_word_results;
}

void opcode_0x09 () {  if (prefix_dd==1)  Writeback_Reg16(REG_IX , ADD_Words(REGISTER_IX , REGISTER_BC) );  else        // add ix,bc
                       if (prefix_fd==1)  Writeback_Reg16(REG_IY , ADD_Words(REGISTER_IY , REGISTER_BC) );  else        // add iy,bc
                                          Writeback_Reg16(REG_HL , ADD_Words(REGISTER_HL , REGISTER_BC) );  return; }   // add hl,bc
                                         
    
void opcode_0x19 () {  if (prefix_dd==1)  Writeback_Reg16(REG_IX , ADD_Words(REGISTER_IX , REGISTER_DE) );  else        // add ix,de
                       if (prefix_fd==1)  Writeback_Reg16(REG_IY , ADD_Words(REGISTER_IY , REGISTER_DE) );  else        // add iy,de
                                          Writeback_Reg16(REG_HL , ADD_Words(REGISTER_HL , REGISTER_DE) );  return; }   // add hl,de
                                         
    
void opcode_0x29 () {  if (prefix_dd==1)  Writeback_Reg16(REG_IX , ADD_Words(REGISTER_IX , REGISTER_IX) );  else        // add ix,ix
                       if (prefix_fd==1)  Writeback_Reg16(REG_IY , ADD_Words(REGISTER_IY , REGISTER_IY) );  else        // add iy,iy
                                          Writeback_Reg16(REG_HL , ADD_Words(REGISTER_HL , REGISTER_HL) );  return; }   // add hl,hl
                                         
    
void opcode_0x39 () {  if (prefix_dd==1)  Writeback_Reg16(REG_IX , ADD_Words(REGISTER_IX , register_sp) );  else        // add ix,sp
                       if (prefix_fd==1)  Writeback_Reg16(REG_IY , ADD_Words(REGISTER_IY , register_sp) );  else        // add iy,sp
                                          Writeback_Reg16(REG_HL , ADD_Words(REGISTER_HL , register_sp) );  return; }   // add hl,sp
                                         
                                         

void opcode_0xC6 () {  register_a = ADD_Bytes(register_a , Fetch_byte() );                       return; }   // add a,*
void opcode_0x87 () {  register_a = ADD_Bytes(register_a , register_a);                          return; }   // add a,a
void opcode_0x80 () {  register_a = ADD_Bytes(register_a , register_b);                          return; }   // add a,b
void opcode_0x81 () {  register_a = ADD_Bytes(register_a , register_c);                          return; }   // add a,c
void opcode_0x82 () {  register_a = ADD_Bytes(register_a , register_d);                          return; }   // add a,d
void opcode_0x83 () {  register_a = ADD_Bytes(register_a , register_e);                          return; }   // add a,e

void opcode_0x84 () {  if (prefix_dd==1) { register_a = ADD_Bytes(register_a , register_ixh); }  else        // add a, h/ixh/iyh
                       if (prefix_fd==1) { register_a = ADD_Bytes(register_a , register_iyh); }  else
                                         { register_a = ADD_Bytes(register_a , register_h);   }  return; }        

void opcode_0x85 () {  if (prefix_dd==1) { register_a = ADD_Bytes(register_a , register_ixl); }  else        // add a, l/ixl/iyl
                       if (prefix_fd==1) { register_a = ADD_Bytes(register_a , register_iyl); }  else
                                         { register_a = ADD_Bytes(register_a , register_l);   }  return; }        

void opcode_0x86 () {  if (prefix_dd==1) { register_a = ADD_Bytes(register_a , Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()))); }  else     // add a, (hl)/(ix+*)/(iy+*)
                       if (prefix_fd==1) { register_a = ADD_Bytes(register_a , Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()))); }  else
                                         { register_a = ADD_Bytes(register_a , Read_byte(REGISTER_HL));   }  return; }        


void opcode_0xED4A() { with_carry=1; Writeback_Reg16(REG_HL , ADD_Words2(REGISTER_HL , REGISTER_BC) );          return; }   // adc hl,bc
void opcode_0xED5A() { with_carry=1; Writeback_Reg16(REG_HL , ADD_Words2(REGISTER_HL , REGISTER_DE) );          return; }   // adc hl,de
void opcode_0xED6A() { with_carry=1; Writeback_Reg16(REG_HL , ADD_Words2(REGISTER_HL , REGISTER_HL) );          return; }   // adc hl,hl
void opcode_0xED7A() { with_carry=1; Writeback_Reg16(REG_HL , ADD_Words2(REGISTER_HL , register_sp) );          return; }   // adc hl,sp

void opcode_0xCE () {  with_carry=1; register_a = ADD_Bytes(register_a , Fetch_byte() );                     return; }   // adc *
void opcode_0x8F () {  with_carry=1; register_a = ADD_Bytes(register_a , register_a);                          return; }   // adc a,a
void opcode_0x88 () {  with_carry=1; register_a = ADD_Bytes(register_a , register_b);                          return; }   // adc a,b
void opcode_0x89 () {  with_carry=1; register_a = ADD_Bytes(register_a , register_c);                          return; }   // adc a,c
void opcode_0x8A () {  with_carry=1; register_a = ADD_Bytes(register_a , register_d);                          return; }   // adc a,d
void opcode_0x8B () {  with_carry=1; register_a = ADD_Bytes(register_a , register_e);                          return; }   // adc a,e

void opcode_0x8C () {  if (prefix_dd==1) { with_carry=1; register_a = ADD_Bytes(register_a , register_ixh); }  else        // adc a, h/ixh/iyh
                       if (prefix_fd==1) { with_carry=1; register_a = ADD_Bytes(register_a , register_iyh); }  else
                                         { with_carry=1; register_a = ADD_Bytes(register_a , register_h);   }  return; }        

void opcode_0x8D () {  if (prefix_dd==1) { with_carry=1; register_a = ADD_Bytes(register_a , register_ixl); }  else        // adc a, l/ixl/iyl
                       if (prefix_fd==1) { with_carry=1; register_a = ADD_Bytes(register_a , register_iyl); }  else
                                         { with_carry=1; register_a = ADD_Bytes(register_a , register_l);   }  return; }        

void opcode_0x8E () {  if (prefix_dd==1) { with_carry=1; register_a = ADD_Bytes(register_a , Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()))); }  else     // adc a, (hl)/(ix+*)/(iy+*)
                       if (prefix_fd==1) { with_carry=1; register_a = ADD_Bytes(register_a , Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()))); }  else
                                         { with_carry=1; register_a = ADD_Bytes(register_a , Read_byte(REGISTER_HL));   }  return; }        


void opcode_0xED44(){  register_a = SUB_Bytes(0 , register_a);                                   return; }    // neg a
void opcode_0xD6 () {  register_a = SUB_Bytes(register_a , Fetch_byte() );                       return; }    // sub a,*
void opcode_0x97 () {  register_a = SUB_Bytes(register_a , register_a);                          return; }    // sub a,a
void opcode_0x90 () {  register_a = SUB_Bytes(register_a , register_b);                          return; }    // sub a,b
void opcode_0x91 () {  register_a = SUB_Bytes(register_a , register_c);                          return; }    // sub a,c
void opcode_0x92 () {  register_a = SUB_Bytes(register_a , register_d);                          return; }    // sub a,d
void opcode_0x93 () {  register_a = SUB_Bytes(register_a , register_e);                          return; }    // sub a,e

void opcode_0x94 () {  if (prefix_dd==1) { register_a = SUB_Bytes(register_a , register_ixh); }  else         // sub a, h/ixh/iyh
                       if (prefix_fd==1) { register_a = SUB_Bytes(register_a , register_iyh); }  else
                                         { register_a = SUB_Bytes(register_a , register_h);   }  return; }        

void opcode_0x95 () {  if (prefix_dd==1) { register_a = SUB_Bytes(register_a , register_ixl); }  else         // sub a, l/ixl/iyl
                       if (prefix_fd==1) { register_a = SUB_Bytes(register_a , register_iyl); }  else
                                         { register_a = SUB_Bytes(register_a , register_l);   }  return; }        

void opcode_0x96 () {  if (prefix_dd==1) { register_a = SUB_Bytes(register_a , Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()))); }  else     // sub a, (hl)/(ix+*)/(iy+*)
                       if (prefix_fd==1) { register_a = SUB_Bytes(register_a , Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()))); }  else
                                         { register_a = SUB_Bytes(register_a , Read_byte(REGISTER_HL));   }  return; }        



void opcode_0xED42() { with_carry=1; Writeback_Reg16(REG_HL , SUB_Words(REGISTER_HL , REGISTER_BC) );          return; }   // sbc hl,bc
void opcode_0xED52() { with_carry=1; Writeback_Reg16(REG_HL , SUB_Words(REGISTER_HL , REGISTER_DE) );          return; }   // sbc hl,de
void opcode_0xED62() { with_carry=1; Writeback_Reg16(REG_HL , SUB_Words(REGISTER_HL , REGISTER_HL) );          return; }   // sbc hl,hl
void opcode_0xED72() { with_carry=1; Writeback_Reg16(REG_HL , SUB_Words(REGISTER_HL , register_sp) );          return; }   // sbc hl,sp

void opcode_0xDE () {  with_carry=1; register_a = SUB_Bytes(register_a , Fetch_byte() );                     return; }    // sbc a,*
void opcode_0x9F () {  with_carry=1; register_a = SUB_Bytes(register_a , register_a);                          return; }    // sbc a,a
void opcode_0x98 () {  with_carry=1; register_a = SUB_Bytes(register_a , register_b);                          return; }    // sbc a,b
void opcode_0x99 () {  with_carry=1; register_a = SUB_Bytes(register_a , register_c);                          return; }    // sbc a,c
void opcode_0x9A () {  with_carry=1; register_a = SUB_Bytes(register_a , register_d);                          return; }    // sbc a,d
void opcode_0x9B () {  with_carry=1; register_a = SUB_Bytes(register_a , register_e);                          return; }    // sbc a,e

void opcode_0x9C () {  if (prefix_dd==1) { with_carry=1; register_a = SUB_Bytes(register_a , register_ixh); }  else         // sbc a, h/ixh/iyh
                       if (prefix_fd==1) { with_carry=1; register_a = SUB_Bytes(register_a , register_iyh); }  else
                                         { with_carry=1; register_a = SUB_Bytes(register_a , register_h);   }  return; }        

void opcode_0x9D () {  if (prefix_dd==1) { with_carry=1; register_a = SUB_Bytes(register_a , register_ixl); }  else         // sbc a, l/ixl/iyl
                       if (prefix_fd==1) { with_carry=1; register_a = SUB_Bytes(register_a , register_iyl); }  else
                                         { with_carry=1; register_a = SUB_Bytes(register_a , register_l);   }  return; }        

void opcode_0x9E () {  if (prefix_dd==1) { with_carry=1; register_a = SUB_Bytes(register_a , Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()))); }  else     // sbc a, (hl)/(ix+*)/(iy+*)
                       if (prefix_fd==1) { with_carry=1; register_a = SUB_Bytes(register_a , Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()))); }  else
                                         { with_carry=1; register_a = SUB_Bytes(register_a , Read_byte(REGISTER_HL));   }  return; }        


void opcode_0xFE () {  cp_opcode=1; SUB_Bytes(register_a , Fetch_byte() );                     return; }    // cp * 
void opcode_0xBF () {  cp_opcode=1; SUB_Bytes(register_a , register_a);                          return; }    // cp a,a
void opcode_0xB8 () {  cp_opcode=1; SUB_Bytes(register_a , register_b);                          return; }    // cp a,b
void opcode_0xB9 () {  cp_opcode=1; SUB_Bytes(register_a , register_c);                          return; }    // cp a,c
void opcode_0xBA () {  cp_opcode=1; SUB_Bytes(register_a , register_d);                          return; }    // cp a,d
void opcode_0xBB () {  cp_opcode=1; SUB_Bytes(register_a , register_e);                          return; }    // cp a,e
     
void opcode_0xBC () {  if (prefix_dd==1) { cp_opcode=1; SUB_Bytes(register_a , register_ixh); }  else         // cp a, h/ixh/iyh
                       if (prefix_fd==1) { cp_opcode=1; SUB_Bytes(register_a , register_iyh); }  else
                                         { cp_opcode=1; SUB_Bytes(register_a , register_h);   }  return; }        
     
void opcode_0xBD () {  if (prefix_dd==1) { cp_opcode=1; SUB_Bytes(register_a , register_ixl); }  else         // cp a, l/ixl/iyl
                       if (prefix_fd==1) { cp_opcode=1; SUB_Bytes(register_a , register_iyl); }  else
                                         { cp_opcode=1; SUB_Bytes(register_a , register_l);   }  return; }        
     
void opcode_0xBE () {  if (prefix_dd==1) { cp_opcode=1; SUB_Bytes(register_a , Read_byte(REGISTER_IX + Sign_Extend(Fetch_byte()))); }  else     // cp a, (hl)/(ix+*)/(iy+*)
                       if (prefix_fd==1) { cp_opcode=1; SUB_Bytes(register_a , Read_byte(REGISTER_IY + Sign_Extend(Fetch_byte()))); }  else
                                         { cp_opcode=1; SUB_Bytes(register_a , Read_byte(REGISTER_HL));   }  return; }        

void opcode_0x33 () {  register_sp++;                                                        return;  }  // inc sp
void opcode_0x03 () {  Writeback_Reg16(REG_BC , (REGISTER_BC) + 1);                          return;  }  // inc bc
void opcode_0x13 () {  Writeback_Reg16(REG_DE , (REGISTER_DE) + 1);                          return;  }  // inc de
void opcode_0x23 () {  if (prefix_dd==1) { Writeback_Reg16(REG_IX , (REGISTER_IX) + 1);  }   else        // inc hl
                       if (prefix_fd==1) { Writeback_Reg16(REG_IY , (REGISTER_IY) + 1);  }   else  
                                         { Writeback_Reg16(REG_HL , (REGISTER_HL) + 1);  }   return;    }


void opcode_0x3C () {  inc_dec=1;  register_a = ADD_Bytes(register_a , 0x1);                              return;  }  // inc a
void opcode_0x04 () {  inc_dec=1;  register_b = ADD_Bytes(register_b , 0x1);                              return;  }  // inc b
void opcode_0x0C () {  inc_dec=1;  register_c = ADD_Bytes(register_c , 0x1);                              return;  }  // inc c
void opcode_0x14 () {  inc_dec=1;  register_d = ADD_Bytes(register_d , 0x1);                              return;  }  // inc d
void opcode_0x1C () {  inc_dec=1;  register_e = ADD_Bytes(register_e , 0x1);                              return;  }  // inc e

void opcode_0x24 () {  inc_dec=1; if (prefix_dd==1) { register_ixh=ADD_Bytes(register_ixh , 0x1);  }   else        // inc ixh, iyh, h
                                  if (prefix_fd==1) { register_iyh=ADD_Bytes(register_iyh , 0x1);  }   else  
                                                    { register_h=ADD_Bytes(register_h   , 0x1);  }   return;    }

void opcode_0x2C () {  inc_dec=1; if (prefix_dd==1) { register_ixl=ADD_Bytes(register_ixl , 0x1);  }   else        // inc ixl, iyl, l
                                  if (prefix_fd==1) { register_iyl=ADD_Bytes(register_iyl , 0x1);  }   else  
                                                    { register_l=ADD_Bytes(register_l   , 0x1);  }   return;    }
                                                    
void opcode_0x34 () {  inc_dec=1; if (prefix_dd==1) { temp16=REGISTER_IX + Sign_Extend(Fetch_byte()); Write_byte(temp16 , ADD_Bytes(Read_byte(temp16),0x1));  }   else        // inc ix+*, iy+*, (hl)
                                  if (prefix_fd==1) { temp16=REGISTER_IY + Sign_Extend(Fetch_byte()); Write_byte(temp16 , ADD_Bytes(Read_byte(temp16),0x1));  }   else  
                                                    { Write_byte(REGISTER_HL , ADD_Bytes(Read_byte(REGISTER_HL),0x1) );  }   return;    }
                                                    

// -----------

void opcode_0x3B () {  register_sp--;                                                        return;  }  // dec sp
void opcode_0x0B () {  Writeback_Reg16(REG_BC , (REGISTER_BC) - 1);                          return;  }  // dec bc
void opcode_0x1B () {  Writeback_Reg16(REG_DE , (REGISTER_DE) - 1);                          return;  }  // dec de
void opcode_0x2B () {  if (prefix_dd==1) { Writeback_Reg16(REG_IX , (REGISTER_IX) - 1);  }   else        // dec hl
                       if (prefix_fd==1) { Writeback_Reg16(REG_IY , (REGISTER_IY) - 1);  }   else  
                                         { Writeback_Reg16(REG_HL , (REGISTER_HL) - 1);  }   return;    }

// -----------

void opcode_0x3D () {  inc_dec=1;  register_a = SUB_Bytes(register_a , 0x1);                              return;  }  // dec a
void opcode_0x05 () {  inc_dec=1;  register_b = SUB_Bytes(register_b , 0x1);                              return;  }  // dec b
void opcode_0x0D () {  inc_dec=1;  register_c = SUB_Bytes(register_c , 0x1);                              return;  }  // dec c
void opcode_0x15 () {  inc_dec=1;  register_d = SUB_Bytes(register_d , 0x1);                              return;  }  // dec d
void opcode_0x1D () {  inc_dec=1;  register_e = SUB_Bytes(register_e , 0x1);                              return;  }  // dec e

void opcode_0x25 () {  inc_dec=1; if (prefix_dd==1) { register_ixh=SUB_Bytes(register_ixh , 0x1);  }   else        // dec ixh, iyh, h
                                  if (prefix_fd==1) { register_iyh=SUB_Bytes(register_iyh , 0x1);  }   else  
                                                    { register_h=SUB_Bytes(register_h   , 0x1);  }   return;    }

void opcode_0x2D () {  inc_dec=1; if (prefix_dd==1) { register_ixl=SUB_Bytes(register_ixl , 0x1);  }   else        // dec ixl, iyl, l
                                  if (prefix_fd==1) { register_iyl=SUB_Bytes(register_iyl , 0x1);  }   else  
                                                    { register_l=SUB_Bytes(register_l   , 0x1);  }   return;    }
                                                    
void opcode_0x35 () {  inc_dec=1; if (prefix_dd==1) { temp16=REGISTER_IX + Sign_Extend(Fetch_byte()); Write_byte(temp16 , SUB_Bytes(Read_byte(temp16),0x1));  }   else        // dec ix+*, iy+*, (hl)
                                  if (prefix_fd==1) { temp16=REGISTER_IY + Sign_Extend(Fetch_byte()); Write_byte(temp16 , SUB_Bytes(Read_byte(temp16),0x1));  }   else  
                                                    { Write_byte(REGISTER_HL , SUB_Bytes(Read_byte(REGISTER_HL),0x1) );  }   return;    }
                                                    




// ------------------------------------------------------
// Input and Output
// ------------------------------------------------------
void Flags_IO(uint8_t local_data) {
    
    register_f = register_f & 0x29;                         // Clear S, Z, H, P, N flags
    register_f = register_f | Parity_Array[local_data];     // Set P flag
    register_f = register_f | (local_data&0x80);            // Set S flag
    if (local_data==0)  register_f = register_f | 0x40;     // Set Z flag
    return;
}
void opcode_0xDB()   {  register_a = BIU_Bus_Cycle(IO_READ_BYTE , Fetch_byte()  , 0x00 );                        return;    }     // in a,(*)
void opcode_0xED78() {  register_a = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_a);  return;    }     // in a,(c)
void opcode_0xED40() {  register_b = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_b);  return;    }     // in b,(c)
void opcode_0xED48() {  register_c = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_c);  return;    }     // in c,(c)
void opcode_0xED50() {  register_d = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_d);  return;    }     // in d,(c)
void opcode_0xED58() {  register_e = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_e);  return;    }     // in e,(c)
void opcode_0xED60() {  register_h = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_h);  return;    }     // in h,(c)
void opcode_0xED68() {  register_l = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(register_l);  return;    }     // in l,(c)
void opcode_0xED70() {  temp8      = BIU_Bus_Cycle(IO_READ_BYTE , register_c      , 0x00 ); Flags_IO(temp8);       return;    }     // in(c)


void opcode_0xD3()     {  BIU_Bus_Cycle(IO_WRITE_BYTE , Fetch_byte() ,            register_a );   return;    }     // out (*),a
void opcode_0xED79()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_a );   return;    }     // out (c),a
void opcode_0xED41()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_b );   return;    }     // out (c),b
void opcode_0xED49()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_c );   return;    }     // out (c),c
void opcode_0xED51()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_d );   return;    }     // out (c),d
void opcode_0xED59()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_e );   return;    }     // out (c),e
void opcode_0xED61()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_h );   return;    }     // out (c),h
void opcode_0xED69()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,              register_l );   return;    }     // out (c),l
void opcode_0xED71()   {  BIU_Bus_Cycle(IO_WRITE_BYTE , register_c ,                     0x0 );   return;    }     // out (c),0x0

        
// ------------------------------------------------------
// Shifts and Rotates
// ------------------------------------------------------

void Flags_Shifts(uint8_t local_data) {
    if (prefix_cb==0) {
        register_f = register_f & 0xC5;                         // Clear H, N, 5, 3 flags 
        register_f = register_f | (local_data&0x28);            // Set flag bits 5,3 to ALU results
    }
    else  {
        register_f = register_f & 0x01;                         // Clear H, N, P, S, Z, 5, 3  flags     
        register_f = register_f | Parity_Array[local_data];     // Set P flag
        register_f = register_f | (local_data&0x80);            // Set S flag
        if (local_data==0)  register_f = register_f | 0x40;     // Set Z flag
        register_f = register_f | (local_data&0x28);            // Set flag bits 5,3 to ALU results
    }
    return;
}


uint8_t RLC(uint8_t local_data) {
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data>>7);              // C Flag = bit[7]
                        local_data = (local_data<<1);                           // Shift register_a left 1 bit
                        local_data = local_data | (register_f&0x01);            // register_a bit[0] = bit shifted out of bit[7]
                        Flags_Shifts(local_data);
                        return local_data;    }     // rlca


uint8_t RRC(uint8_t local_data) {
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data&0x01);            // C Flag = bit[0]
                        local_data = (local_data>>1);                           // Shift register_a left 1 bit
                        local_data = local_data | ((register_f&0x01)<<7);       // register_a bit[7] = bit shifted out of bit[0]
                        Flags_Shifts(local_data);  
                        return local_data;    }     // rrca



uint8_t RL(uint8_t local_data) {
                        temp8      = register_f & 0x01;                         // Store old C flag
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data>>7);              // C Flag = bit[7]
                        local_data = (local_data<<1);                           // Shift register_a left 1 bit
                        local_data = local_data | temp8;                        // register_a bit[0] = old C Flag
                        Flags_Shifts(local_data);
                        return local_data;    }     // rla



uint8_t RR(uint8_t local_data) {
                        temp8      = register_f & 0x01;                         // Store old C flag
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data&0x01);            // C Flag = bit[0]
                        local_data = (local_data>>1);                           // Shift register_a left 1 bit
                        local_data = local_data | (temp8<<7);                   // register_a bit[7] = old C Flag
                        Flags_Shifts(local_data);
                        return local_data;    }     // rra


uint8_t SLA(uint8_t local_data) {
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data>>7);              // C Flag = bit[7]
                        local_data = (local_data<<1);                           // Shift register_a left 1 bit
                        Flags_Shifts(local_data);
                        return local_data;    }     // sla
                        
    
uint8_t SRA(uint8_t local_data) {
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data&0x01);            // C Flag = bit[0]
                        local_data = (local_data>>1);                           // Shift register_a right 1 bit
                        if (local_data&0x40) local_data=local_data | 0x80;      // Keep bit[7] the same as before the shift
                        Flags_Shifts(local_data);
                        return local_data;    }     // sra

                        

uint8_t SLL(uint8_t local_data) {
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data>>7);              // C Flag = bit[7]
                        local_data = (local_data<<1);                           // Shift register_a left 1 bit
                        local_data = local_data | 0x1;                          // Set bit[0] to 1
                        Flags_Shifts(local_data);
                        return local_data;    }     // sll



uint8_t SRL(uint8_t local_data) {
                        register_f = register_f & 0xFE;                         // Clear C flag
                        register_f = register_f | (local_data&0x01);            // C Flag = bit[0]
                        local_data = (local_data>>1);                           // Shift register_a left 1 bit
                        Flags_Shifts(local_data);
                        return local_data;    }     // rra


void opcode_0x07()   {register_a = RLC(register_a);   return;    }     // rlc
void opcode_0xCB00() {if (prefix_dd==1) {register_b = RLC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = RLC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=RLC(register_b); }return;    }    
void opcode_0xCB01() {if (prefix_dd==1) {register_c = RLC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = RLC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=RLC(register_c); }return;    }     
void opcode_0xCB02() {if (prefix_dd==1) {register_d = RLC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = RLC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=RLC(register_d); }return;    }     
void opcode_0xCB03() {if (prefix_dd==1) {register_e = RLC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = RLC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=RLC(register_e); }return;    }     
void opcode_0xCB04() {if (prefix_dd==1) {register_h = RLC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = RLC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=RLC(register_h); }return;    }     
void opcode_0xCB05() {if (prefix_dd==1) {register_l = RLC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = RLC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=RLC(register_l); }return;    }     
void opcode_0xCB07() {if (prefix_dd==1) {register_a = RLC(Read_byte(REGISTER_IX+Sign_Extend(cb_prefix_offset)));}  else if (prefix_fd==1) {register_a = RLC(Read_byte(REGISTER_IY+Sign_Extend(cb_prefix_offset)));} else {register_a=RLC(register_a); }return;    }     
void opcode_0xCB06() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RLC(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RLC(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , RLC(Read_byte(REGISTER_HL)) );return;    }  

void opcode_0x0F()   {register_a = RRC(register_a);   return;    }     // rrc
void opcode_0xCB08() {if (prefix_dd==1) {register_b = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=RRC(register_b); }return;    }    
void opcode_0xCB09() {if (prefix_dd==1) {register_c = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=RRC(register_c); }return;    }     
void opcode_0xCB0A() {if (prefix_dd==1) {register_d = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=RRC(register_d); }return;    }     
void opcode_0xCB0B() {if (prefix_dd==1) {register_e = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=RRC(register_e); }return;    }     
void opcode_0xCB0C() {if (prefix_dd==1) {register_h = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=RRC(register_h); }return;    }     
void opcode_0xCB0D() {if (prefix_dd==1) {register_l = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=RRC(register_l); }return;    }     
void opcode_0xCB0F() {if (prefix_dd==1) {register_a = RRC(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = RRC(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=RRC(register_a); }return;    }     
void opcode_0xCB0E() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RRC(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RRC(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , RRC(Read_byte(REGISTER_HL)) );return;    }  

void opcode_0x17()   {register_a = RL(register_a);   return;    }     // rl
void opcode_0xCB10() {if (prefix_dd==1) {register_b = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=RL(register_b); }return;    }    
void opcode_0xCB11() {if (prefix_dd==1) {register_c = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=RL(register_c); }return;    }     
void opcode_0xCB12() {if (prefix_dd==1) {register_d = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=RL(register_d); }return;    }     
void opcode_0xCB13() {if (prefix_dd==1) {register_e = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=RL(register_e); }return;    }     
void opcode_0xCB14() {if (prefix_dd==1) {register_h = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=RL(register_h); }return;    }     
void opcode_0xCB15() {if (prefix_dd==1) {register_l = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=RL(register_l); }return;    }     
void opcode_0xCB17() {if (prefix_dd==1) {register_a = RL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = RL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=RL(register_a); }return;    }     
void opcode_0xCB16() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RL(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RL(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , RL(Read_byte(REGISTER_HL)) );return;    }  

void opcode_0x1F()   {register_a = RR(register_a);   return;    }     // rr
void opcode_0xCB18() {if (prefix_dd==1) {register_b = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=RR(register_b); }return;    }    
void opcode_0xCB19() {if (prefix_dd==1) {register_c = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=RR(register_c); }return;    }     
void opcode_0xCB1A() {if (prefix_dd==1) {register_d = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=RR(register_d); }return;    }     
void opcode_0xCB1B() {if (prefix_dd==1) {register_e = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=RR(register_e); }return;    }     
void opcode_0xCB1C() {if (prefix_dd==1) {register_h = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=RR(register_h); }return;    }     
void opcode_0xCB1D() {if (prefix_dd==1) {register_l = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=RR(register_l); }return;    }     
void opcode_0xCB1F() {if (prefix_dd==1) {register_a = RR(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = RR(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=RR(register_a); }return;    }     
void opcode_0xCB1E() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RR(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RR(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , RR(Read_byte(REGISTER_HL)) );return;    }  

// ----


void opcode_0xCB20() {if (prefix_dd==1) {register_b = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=SLA(register_b); }return;    }    
void opcode_0xCB21() {if (prefix_dd==1) {register_c = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=SLA(register_c); }return;    }     
void opcode_0xCB22() {if (prefix_dd==1) {register_d = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=SLA(register_d); }return;    }     
void opcode_0xCB23() {if (prefix_dd==1) {register_e = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=SLA(register_e); }return;    }     
void opcode_0xCB24() {if (prefix_dd==1) {register_h = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=SLA(register_h); }return;    }     
void opcode_0xCB25() {if (prefix_dd==1) {register_l = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=SLA(register_l); }return;    }     
void opcode_0xCB27() {if (prefix_dd==1) {register_a = SLA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = SLA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=SLA(register_a); }return;    }     
void opcode_0xCB26() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SLA(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SLA(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , SLA(Read_byte(REGISTER_HL)) );return;    }  

void opcode_0xCB28() {if (prefix_dd==1) {register_b = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=SRA(register_b); }return;    }    
void opcode_0xCB29() {if (prefix_dd==1) {register_c = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=SRA(register_c); }return;    }     
void opcode_0xCB2A() {if (prefix_dd==1) {register_d = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=SRA(register_d); }return;    }     
void opcode_0xCB2B() {if (prefix_dd==1) {register_e = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=SRA(register_e); }return;    }     
void opcode_0xCB2C() {if (prefix_dd==1) {register_h = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=SRA(register_h); }return;    }     
void opcode_0xCB2D() {if (prefix_dd==1) {register_l = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=SRA(register_l); }return;    }     
void opcode_0xCB2F() {if (prefix_dd==1) {register_a = SRA(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = SRA(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=SRA(register_a); }return;    }     
void opcode_0xCB2E() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SRA(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SRA(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , SRA(Read_byte(REGISTER_HL)) );return;    }  

void opcode_0xCB30() {if (prefix_dd==1) {register_b = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=SLL(register_b); }return;    }    
void opcode_0xCB31() {if (prefix_dd==1) {register_c = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=SLL(register_c); }return;    }     
void opcode_0xCB32() {if (prefix_dd==1) {register_d = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=SLL(register_d); }return;    }     
void opcode_0xCB33() {if (prefix_dd==1) {register_e = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=SLL(register_e); }return;    }     
void opcode_0xCB34() {if (prefix_dd==1) {register_h = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=SLL(register_h); }return;    }     
void opcode_0xCB35() {if (prefix_dd==1) {register_l = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=SLL(register_l); }return;    }     
void opcode_0xCB37() {if (prefix_dd==1) {register_a = SLL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = SLL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=SLL(register_a); }return;    }     
void opcode_0xCB36() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SLL(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SLL(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , SLL(Read_byte(REGISTER_HL)) );return;    }  

void opcode_0xCB38() {if (prefix_dd==1) {register_b = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_b = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=SRL(register_b); }return;    }    
void opcode_0xCB39() {if (prefix_dd==1) {register_c = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_c = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=SRL(register_c); }return;    }     
void opcode_0xCB3A() {if (prefix_dd==1) {register_d = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_d = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=SRL(register_d); }return;    }     
void opcode_0xCB3B() {if (prefix_dd==1) {register_e = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_e = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=SRL(register_e); }return;    }     
void opcode_0xCB3C() {if (prefix_dd==1) {register_h = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_h = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=SRL(register_h); }return;    }     
void opcode_0xCB3D() {if (prefix_dd==1) {register_l = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_l = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=SRL(register_l); }return;    }     
void opcode_0xCB3F() {if (prefix_dd==1) {register_a = SRL(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {register_a = SRL(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=SRL(register_a); }return;    }     
void opcode_0xCB3E() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SRL(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SRL(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , SRL(Read_byte(REGISTER_HL)) );return;    }  


void BIT(uint8_t local_data) {
    register_f           = register_f & 0x01;                         // Clear all flags except C
    uint8_t local_bitnum = (CB_opcode >> 3) & 0x07;
    switch (local_bitnum)  {
        case 0x00: temp8=(local_data&0x01); if (temp8==0) { register_f = register_f | 0x40; } break;   // Set Z flag
        case 0x01: temp8=(local_data&0x02); if (temp8==0) { register_f = register_f | 0x40; } break;  
        case 0x02: temp8=(local_data&0x04); if (temp8==0) { register_f = register_f | 0x40; } break;  
        case 0x03: temp8=(local_data&0x08); if (temp8==0) { register_f = register_f | 0x40; } break;  
        case 0x04: temp8=(local_data&0x10); if (temp8==0) { register_f = register_f | 0x40; } break;  
        case 0x05: temp8=(local_data&0x20); if (temp8==0) { register_f = register_f | 0x40; } break;  
        case 0x06: temp8=(local_data&0x40); if (temp8==0) { register_f = register_f | 0x40; } break;  
        case 0x07: temp8=(local_data&0x80); if (temp8==0) { register_f = register_f | 0x40; } break;  
    }
    
    register_f = register_f | 0x10;                         // Set H flag
    register_f = register_f | (temp8&0x80);                 // Set S flag
    if (temp8==0) register_f=register_f|0x04;               // Set P flag


    if (special==1)  {                                      // Special handlinig for (hl)
        clock_counter=clock_counter+0x4;
        register_f = register_f | ((register_pc>>8)&0x28);  // Set flag bits 5,3 to ALU results
    }
    else if (special==2)  {                                 // Special handlinig for IX+*
        register_f = register_f | ((temp16>>8)&0x28);       // Set flag bits 5,3 to ALU results
    }
    
    else  {                                                 // Other opcodes
        register_f = register_f | (local_data&0x28);        // Set flag bits 5,3 to ALU results
    }
    
    special=0;
}

void opcode_0xCB_Bit_b()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_b); }return;    }    
void opcode_0xCB_Bit_c()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_c); }return;    }    
void opcode_0xCB_Bit_d()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_d); }return;    }    
void opcode_0xCB_Bit_e()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_e); }return;    }    
void opcode_0xCB_Bit_h()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_h); }return;    }    
void opcode_0xCB_Bit_l()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_l); }return;    }    
void opcode_0xCB_Bit_a()  {if (prefix_dd==1) {BIT(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {BIT(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {BIT(register_a); }return;    }    
void opcode_0xCB_Bit_hl() {if (prefix_dd==1) {special=2; temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); BIT(Read_byte(temp16));}  else if (prefix_fd==1) {special=2; temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); BIT(Read_byte(temp16));} else {special=1; BIT(Read_byte(REGISTER_HL)); }return;    }    


uint8_t RES(uint8_t local_data) {
    uint8_t local_bitnum = (CB_opcode >> 3) & 0x07;
    switch (local_bitnum)  {
        case 0x00:  local_data = (local_data & (~(0x01)));  break;   
        case 0x01:  local_data = (local_data & (~(0x02)));  break;   
        case 0x02:  local_data = (local_data & (~(0x04)));  break;   
        case 0x03:  local_data = (local_data & (~(0x08)));  break;   
        case 0x04:  local_data = (local_data & (~(0x10)));  break;   
        case 0x05:  local_data = (local_data & (~(0x20)));  break;   
        case 0x06:  local_data = (local_data & (~(0x40)));  break;   
        case 0x07:  local_data = (local_data & (~(0x80)));  break;   
    }
    return local_data;
}

void opcode_0xCB_Res_b()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=RES(register_b); }return;    }    
void opcode_0xCB_Res_c()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=RES(register_c); }return;    }    
void opcode_0xCB_Res_d()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=RES(register_d); }return;    }    
void opcode_0xCB_Res_e()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=RES(register_e); }return;    }    
void opcode_0xCB_Res_h()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=RES(register_h); }return;    }    
void opcode_0xCB_Res_l()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=RES(register_l); }return;    }    
void opcode_0xCB_Res_a()  {if (prefix_dd==1) {RES(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {RES(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=RES(register_a); }return;    }    
void opcode_0xCB_Res_hl() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RES(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,RES(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , RES(Read_byte(REGISTER_HL)) );return;    }  


uint8_t SET(uint8_t local_data) {
    clock_counter        = clock_counter+0x3;
    uint8_t local_bitnum = (CB_opcode >> 3) & 0x07;
    switch (local_bitnum)  {
        case 0x00:  local_data = (local_data | 0x01);  break;   
        case 0x01:  local_data = (local_data | 0x02);  break;   
        case 0x02:  local_data = (local_data | 0x04);  break;   
        case 0x03:  local_data = (local_data | 0x08);  break;   
        case 0x04:  local_data = (local_data | 0x10);  break;   
        case 0x05:  local_data = (local_data | 0x20);  break;   
        case 0x06:  local_data = (local_data | 0x40);  break;   
        case 0x07:  local_data = (local_data | 0x80);  break;   
    }
    return local_data;
}

void opcode_0xCB_Set_b()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_b=SET(register_b); }return;    }    
void opcode_0xCB_Set_c()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_c=SET(register_c); }return;    }    
void opcode_0xCB_Set_d()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_d=SET(register_d); }return;    }    
void opcode_0xCB_Set_e()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_e=SET(register_e); }return;    }    
void opcode_0xCB_Set_h()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_h=SET(register_h); }return;    }    
void opcode_0xCB_Set_l()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_l=SET(register_l); }return;    }    
void opcode_0xCB_Set_a()  {if (prefix_dd==1) {SET(Read_byte(REGISTER_IX+Sign_Extend(Fetch_byte())));}  else if (prefix_fd==1) {SET(Read_byte(REGISTER_IY+Sign_Extend(Fetch_byte())));} else {register_a=SET(register_a); }return;    }    
void opcode_0xCB_Set_hl() {if (prefix_dd==1) {temp16=REGISTER_IX+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SET(Read_byte(temp16)));}  else if (prefix_fd==1){temp16=REGISTER_IY+Sign_Extend(cb_prefix_offset); Write_byte(temp16,SET(Read_byte(temp16)));} else Write_byte( (REGISTER_HL) , SET(Read_byte(REGISTER_HL)) );return;    }  


void opcode_0x76()  { // Halt
    DigitalWriteFast(PIN::HALT,0x0);
    halt_in_progress=1;
    register_pc--;
}



void opcode_0x27()  {                                           // daa
    register_f = register_f & 0x13;                             // Clear S, Z, P Flags
    temp8=0;
    if ( ((0x0f&register_a) > 0x09) || (flag_h==1) )  {  temp8=temp8+0x06;  }
    if ( (     (register_a) > 0x99) || (flag_c==1) )  {  temp8=temp8+0x60;  register_f=register_f|0x01; } // Set C Flag
    
    if (flag_n==1)  {
        if (( (0x0F&register_a) < 0x06) && (flag_h==1) )   register_f = register_f | 0x10; else register_f = register_f & 0xEF;  // Set H Flag
        register_a = register_a - temp8;
    }
    else
    {
        if ( (0x0F&register_a) > 0x09 )  register_f = register_f | 0x10; else register_f = register_f & 0xEF;  // Set H Flag
        register_a = register_a + temp8;
    }
    

    if (register_a&0x80)  register_f = register_f | 0x80;       // Set S Flag
    if (register_a==0)         register_f = register_f | 0x40;  // Set Z Flag
    register_f = register_f | Parity_Array[register_a];         // Set P flag   
    register_f = register_f | (register_a&0x28);                // Set flag bits 5,3 to ALU results
    return;
}   


void opcode_0xEDA2()  {                                     // ini
    register_f = register_f & 0xBF;                         // Clear Z flag     
    Write_byte( REGISTER_HL ,  BIU_Bus_Cycle(IO_READ_BYTE,register_c,0x00) );
    Writeback_Reg16( REG_HL , (REGISTER_HL + 1) );
    register_b--;
    if (register_b==0) register_f = register_f | 0x40;      // Set Z flag
    register_f = register_f | 0x02;                         // Set N flag       
    if ( (opcode_byte>0xAF) && (register_b!=0) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2; }    // Allow for repeat of this instruction
    return;}

void opcode_0xEDA3()  {                                     // outi
    register_f = register_f & 0xBF;                         // Clear Z flag     
    BIU_Bus_Cycle(IO_WRITE_BYTE,register_c, (Read_byte(REGISTER_HL)) );
    Writeback_Reg16( REG_HL , (REGISTER_HL + 1) );
    register_b--;
    if (register_b==0) register_f = register_f | 0x40;      // Set Z flag
    register_f = register_f | 0x02;                         // Set N flag       
    if ( (opcode_byte>0xAF) && (register_b!=0) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2; }    // Allow for repeat of this instruction
    return;}
    

void opcode_0xEDA0()  {                                     // ldi
    register_f = register_f & 0xC1;                         // Clear N, H, P, 5,3 flags   
    temp8 = Read_byte(REGISTER_HL);
    Write_byte( REGISTER_DE ,  temp8);
    Writeback_Reg16( REG_HL , (REGISTER_HL + 1) );
    Writeback_Reg16( REG_DE , (REGISTER_DE + 1) );
    Writeback_Reg16( REG_BC , (REGISTER_BC - 1) );
    if ( (opcode_byte>0xAF) && (REGISTER_BC!=0) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2; }       // Allow for repeat of this instruction
    temp8 = temp8 + register_a;
    if (temp8&0x08) register_f = register_f | 0x08;
    if (temp8&0x02) register_f = register_f | 0x20;
    if ( REGISTER_BC!=0) register_f = register_f | 0x04;     // Set P flag
    return;
}

void opcode_0xEDA8()  {                                     // ldd
    register_f = register_f & 0xC1;                         // Clear N, H, P, 5,3 flags   
    temp8 = Read_byte(REGISTER_HL);
    Write_byte( REGISTER_DE ,  temp8);
    Writeback_Reg16( REG_HL , (REGISTER_HL - 1) );
    Writeback_Reg16( REG_DE , (REGISTER_DE - 1) );
    Writeback_Reg16( REG_BC , (REGISTER_BC - 1) );
    if ( (opcode_byte>0xAF) && (REGISTER_BC!=0) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2; }       // Allow for repeat of this instruction
    temp8 = temp8 + register_a;
    if (temp8&0x08) register_f = register_f | 0x08;
    if (temp8&0x02) register_f = register_f | 0x20;
    if ( REGISTER_BC!=0) register_f = register_f | 0x04;     // Set P flag 
    return;
}


void opcode_0xEDA1()  {                                     // cpi  
    uint8_t old_C = flag_c;
    temp8 = SUB_Bytes(register_a , Read_byte(REGISTER_HL) );
    Writeback_Reg16( REG_HL , (REGISTER_HL + 1) );
    Writeback_Reg16( REG_BC , (REGISTER_BC - 1) );
    if ( (opcode_byte>0xAF) && (REGISTER_BC!=0) && (flag_z!=1) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2; }    // Allow for repeat of this instruction
    
    register_f = register_f & 0xD2;                          // Clear C, P, 3,5 flags   
    if ((temp8-flag_h)&0x08) register_f = register_f | 0x08; // 3
    if ((temp8-flag_h)&0x02) register_f = register_f | 0x20; // 5
    if (old_C==1)            register_f = register_f | 0x01; // C
    if (REGISTER_BC!=0)      register_f = register_f | 0x04; // p   
    return;}


void opcode_0xEDA9()  {                                     // cpd
    uint8_t old_C = flag_c;
    temp8 = SUB_Bytes(register_a , Read_byte(REGISTER_HL) );
    Writeback_Reg16( REG_HL , (REGISTER_HL - 1) );
    Writeback_Reg16( REG_BC , (REGISTER_BC - 1) );
    if ( (opcode_byte>0xAF) && (REGISTER_BC!=0) && (flag_z!=1) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2;  }    // Allow for repeat of this instruction
    
    register_f = register_f & 0xD2;                          // Clear C, P, 3,5 flags   
    if ((temp8-flag_h)&0x08) register_f = register_f | 0x08; // 3
    if ((temp8-flag_h)&0x02) register_f = register_f | 0x20; // 5
    if (old_C==1)            register_f = register_f | 0x01; // C
    if (REGISTER_BC!=0)      register_f = register_f | 0x04; // p   
    return;}


void opcode_0xEDAA()  {                                     // ind
    register_f = register_f & 0xBF;                         // Clear Z flag     
    Write_byte( REGISTER_HL ,  BIU_Bus_Cycle(IO_READ_BYTE,register_c,0x00) );
    Writeback_Reg16( REG_HL , (REGISTER_HL + 1) );
    register_b--;
    if (register_b==0) register_f = register_f | 0x40;      // Set Z flag
    register_f = register_f | 0x02;                         // Set N flag       
    if ( (opcode_byte>0xAF) && (register_b!=0) ) {clock_counter=clock_counter+0x5; register_pc=register_pc-2; }    // Allow for repeat of this instruction
    return;}


void opcode_0xEDAB()  {                                     // outd
    register_f = register_f & 0xBF;                         // Clear Z flag     
    BIU_Bus_Cycle(IO_WRITE_BYTE,register_c, (Read_byte(REGISTER_HL)) );
    Writeback_Reg16( REG_HL , (REGISTER_HL - 1) );
    register_b--;
    if (register_b==0) register_f = register_f | 0x40;      // Set Z flag
    register_f = register_f | 0x02;                         // Set N flag       
    if ( (opcode_byte>0xAF) && (register_b!=0) ) register_pc=register_pc-2;     // Allow for repeat of this instruction
    return;}

void opcode_0xED67()  {                                     // rrd
    register_f = register_f & 0x01;                         // Clear S, Z, H, P, N flags
    temp8 = Read_byte(REGISTER_HL);
    temp16 = (register_a<<8) | temp8 ;
    temp16 = temp16 >> 4;
    temp16 = (temp16&0xF0FF) | ((temp8&0x0F)<<8) ;
    
    register_a = (register_a&0xF0) | (0x0F& (temp16>>8));
    Write_byte( REGISTER_HL , (temp16&0x00FF) );
    
    register_f = register_f | Parity_Array[register_a];     // Set P flag  
    register_f = register_f | (register_a&0x80);            // Set S flag
    if (register_a==0)  register_f = register_f | 0x40;     // Set Z flag
    register_f = register_f | (register_a&0x28);            // Set flag bits 5,3 to ALU results
    return;}

void opcode_0xED6F()  {                                     // rld
    register_f = register_f & 0x01;                         // Clear S, Z, H, P, N flags
    
    temp16 = (register_a<<8) | Read_byte(REGISTER_HL) ;
    temp16 = temp16 << 4;
    temp16 = (temp16>>12) | (temp16&0xFFF0) ;
    
    Write_byte( REGISTER_HL , (temp16&0x00FF) );
    register_a = (register_a&0xF0) | (0x0F& (temp16>>8));
    
    register_f = register_f | Parity_Array[register_a];     // Set P flag  
    register_f = register_f | (register_a&0x80);            // Set S flag
    if (register_a==0)  register_f = register_f | 0x40;     // Set Z flag
    register_f = register_f | (register_a&0x28);            // Set flag bits 5,3 to ALU results
    return;}


// -------------------------------------------------
// NMI Handler     
// -------------------------------------------------
void NMI_Handler() {
    
    BIU_Bus_Cycle(OPCODE_READ_M1 , 0xABCD , 0x00 );
    wait_for_CLK_falling_edge();
    
    if (halt_in_progress==1) Push(register_pc+1);  else Push(register_pc);
    halt_in_progress=0;
    DigitalWriteFast(PIN::HALT,0x1);  
    register_iff2 = register_iff1;
    register_iff1 = 0;
    register_pc = 0x0066;
    nmi.ClearLatch();
    return;
}

// -------------------------------------------------
// INTR Handler     
// -------------------------------------------------
void INTR_Handler() {
    uint8_t local_intr_vector;
    
    register_iff1 = 0;
    register_iff2 = 0;  
    
    if (register_im==0)  {
        assert_iack_type0 = 1;      // BIU performs IACK during the next opcode fetch
        return;
    }
    else if (register_im==1)  {
        local_intr_vector  = BIU_Bus_Cycle(INTERRUPT_ACK , 0x0000 , 0x00    ); 
        BIU_Bus_Cycle(OPCODE_READ_M1 , 0xABCD , 0x00 );
        wait_for_CLK_falling_edge();

        if (halt_in_progress==1) {
            Push(register_pc+1);
        } else {
            Push(register_pc);
        }
        halt_in_progress=0;
        DigitalWriteFast(PIN::HALT,0x1);
        register_pc = 0x0038;
        return; 
    }       
    else if (register_im==2)  {
        local_intr_vector  = BIU_Bus_Cycle(INTERRUPT_ACK , 0x0000 , 0x00    ); 
        wait_for_CLK_falling_edge();
        if (halt_in_progress==1) {
            Push(register_pc+1);
        } else {
            Push(register_pc);
        }
        halt_in_progress=0;
        DigitalWriteFast(PIN::HALT,0x1);
        temp16 =  (register_i<<8) | local_intr_vector;
        register_pc = Read_word(temp16);
        return; 
    }       
    
    return;
}
  

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

void decode_table_0xCB()  {

    if (nmi.Latched())  {        // NMI allowed between CB and ED opcodes.  Back-off PC and handle the NMI
        register_pc--;
        return;
    }
  
    prefix_cb=1;

    
    if ( (prefix_dd==1) || (prefix_fd==1)  )  { 
        //cb_prefix_offset = Fetch_opcode();
        cb_prefix_offset = Fetch_byte();
        opcode_byte      = Fetch_opcode();
        CB_opcode = opcode_byte;
    }
    else  {
        opcode_byte = Fetch_opcode();
        CB_opcode = opcode_byte;        
    }

    switch (opcode_byte)  {
        
       case 0x00:   opcode_0xCB00();        break; 
       case 0x01:   opcode_0xCB01();        break; 
       case 0x02:   opcode_0xCB02();        break; 
       case 0x03:   opcode_0xCB03();        break; 
       case 0x04:   opcode_0xCB04();        break; 
       case 0x05:   opcode_0xCB05();        break; 
       case 0x06:   opcode_0xCB06();        break; 
       case 0x07:   opcode_0xCB07();        break; 
       case 0x08:   opcode_0xCB08();        break; 
       case 0x09:   opcode_0xCB09();        break; 
       case 0x0A:   opcode_0xCB0A();        break; 
       case 0x0B:   opcode_0xCB0B();        break; 
       case 0x0C:   opcode_0xCB0C();        break; 
       case 0x0D:   opcode_0xCB0D();        break; 
       case 0x0E:   opcode_0xCB0E();        break; 
       case 0x0F:   opcode_0xCB0F();        break; 
       case 0x10:   opcode_0xCB10();        break; 
       case 0x11:   opcode_0xCB11();        break; 
       case 0x12:   opcode_0xCB12();        break; 
       case 0x13:   opcode_0xCB13();        break; 
       case 0x14:   opcode_0xCB14();        break; 
       case 0x15:   opcode_0xCB15();        break; 
       case 0x16:   opcode_0xCB16();        break; 
       case 0x17:   opcode_0xCB17();        break; 
       case 0x18:   opcode_0xCB18();        break; 
       case 0x19:   opcode_0xCB19();        break; 
       case 0x1A:   opcode_0xCB1A();        break; 
       case 0x1B:   opcode_0xCB1B();        break; 
       case 0x1C:   opcode_0xCB1C();        break; 
       case 0x1D:   opcode_0xCB1D();        break; 
       case 0x1E:   opcode_0xCB1E();        break; 
       case 0x1F:   opcode_0xCB1F();        break; 
       case 0x20:   opcode_0xCB20();        break; 
       case 0x21:   opcode_0xCB21();        break; 
       case 0x22:   opcode_0xCB22();        break; 
       case 0x23:   opcode_0xCB23();        break; 
       case 0x24:   opcode_0xCB24();        break; 
       case 0x25:   opcode_0xCB25();        break; 
       case 0x26:   opcode_0xCB26();        break; 
       case 0x27:   opcode_0xCB27();        break; 
       case 0x28:   opcode_0xCB28();        break; 
       case 0x29:   opcode_0xCB29();        break; 
       case 0x2A:   opcode_0xCB2A();        break; 
       case 0x2B:   opcode_0xCB2B();        break; 
       case 0x2C:   opcode_0xCB2C();        break; 
       case 0x2D:   opcode_0xCB2D();        break; 
       case 0x2E:   opcode_0xCB2E();        break; 
       case 0x2F:   opcode_0xCB2F();        break; 
       case 0x30:   opcode_0xCB30();        break; 
       case 0x31:   opcode_0xCB31();        break; 
       case 0x32:   opcode_0xCB32();        break; 
       case 0x33:   opcode_0xCB33();        break; 
       case 0x34:   opcode_0xCB34();        break; 
       case 0x35:   opcode_0xCB35();        break; 
       case 0x36:   opcode_0xCB36();        break; 
       case 0x37:   opcode_0xCB37();        break; 
       case 0x38:   opcode_0xCB38();        break; 
       case 0x39:   opcode_0xCB39();        break; 
       case 0x3A:   opcode_0xCB3A();        break; 
       case 0x3B:   opcode_0xCB3B();        break; 
       case 0x3C:   opcode_0xCB3C();        break; 
       case 0x3D:   opcode_0xCB3D();        break; 
       case 0x3E:   opcode_0xCB3E();        break; 
       case 0x3F:   opcode_0xCB3F();        break; 
       case 0x40:   opcode_0xCB_Bit_b();    break; 
       case 0x41:   opcode_0xCB_Bit_c();    break; 
       case 0x42:   opcode_0xCB_Bit_d();    break; 
       case 0x43:   opcode_0xCB_Bit_e();    break; 
       case 0x44:   opcode_0xCB_Bit_h();    break; 
       case 0x45:   opcode_0xCB_Bit_l();    break; 
       case 0x46:   opcode_0xCB_Bit_hl();   break; 
       case 0x47:   opcode_0xCB_Bit_a();    break; 
       case 0x48:   opcode_0xCB_Bit_b();    break; 
       case 0x49:   opcode_0xCB_Bit_c();    break; 
       case 0x4A:   opcode_0xCB_Bit_d();    break; 
       case 0x4B:   opcode_0xCB_Bit_e();    break; 
       case 0x4C:   opcode_0xCB_Bit_h();    break; 
       case 0x4D:   opcode_0xCB_Bit_l();    break; 
       case 0x4E:   opcode_0xCB_Bit_hl();   break; 
       case 0x4F:   opcode_0xCB_Bit_a();    break; 
       case 0x50:   opcode_0xCB_Bit_b();    break; 
       case 0x51:   opcode_0xCB_Bit_c();    break; 
       case 0x52:   opcode_0xCB_Bit_d();    break; 
       case 0x53:   opcode_0xCB_Bit_e();    break; 
       case 0x54:   opcode_0xCB_Bit_h();    break; 
       case 0x55:   opcode_0xCB_Bit_l();    break; 
       case 0x56:   opcode_0xCB_Bit_hl();   break; 
       case 0x57:   opcode_0xCB_Bit_a();    break; 
       case 0x58:   opcode_0xCB_Bit_b();    break; 
       case 0x59:   opcode_0xCB_Bit_c();    break; 
       case 0x5A:   opcode_0xCB_Bit_d();    break; 
       case 0x5B:   opcode_0xCB_Bit_e();    break; 
       case 0x5C:   opcode_0xCB_Bit_h();    break; 
       case 0x5D:   opcode_0xCB_Bit_l();    break; 
       case 0x5E:   opcode_0xCB_Bit_hl();   break; 
       case 0x5F:   opcode_0xCB_Bit_a();    break; 
       case 0x60:   opcode_0xCB_Bit_b();    break; 
       case 0x61:   opcode_0xCB_Bit_c();    break; 
       case 0x62:   opcode_0xCB_Bit_d();    break; 
       case 0x63:   opcode_0xCB_Bit_e();    break; 
       case 0x64:   opcode_0xCB_Bit_h();    break; 
       case 0x65:   opcode_0xCB_Bit_l();    break; 
       case 0x66:   opcode_0xCB_Bit_hl();   break; 
       case 0x67:   opcode_0xCB_Bit_a();    break; 
       case 0x68:   opcode_0xCB_Bit_b();    break; 
       case 0x69:   opcode_0xCB_Bit_c();    break; 
       case 0x6A:   opcode_0xCB_Bit_d();    break; 
       case 0x6B:   opcode_0xCB_Bit_e();    break; 
       case 0x6C:   opcode_0xCB_Bit_h();    break; 
       case 0x6D:   opcode_0xCB_Bit_l();    break; 
       case 0x6E:   opcode_0xCB_Bit_hl();   break; 
       case 0x6F:   opcode_0xCB_Bit_a();    break; 
       case 0x70:   opcode_0xCB_Bit_b();    break; 
       case 0x71:   opcode_0xCB_Bit_c();    break; 
       case 0x72:   opcode_0xCB_Bit_d();    break; 
       case 0x73:   opcode_0xCB_Bit_e();    break; 
       case 0x74:   opcode_0xCB_Bit_h();    break; 
       case 0x75:   opcode_0xCB_Bit_l();    break; 
       case 0x76:   opcode_0xCB_Bit_hl();   break; 
       case 0x77:   opcode_0xCB_Bit_a();    break; 
       case 0x78:   opcode_0xCB_Bit_b();    break; 
       case 0x79:   opcode_0xCB_Bit_c();    break; 
       case 0x7A:   opcode_0xCB_Bit_d();    break; 
       case 0x7B:   opcode_0xCB_Bit_e();    break; 
       case 0x7C:   opcode_0xCB_Bit_h();    break; 
       case 0x7D:   opcode_0xCB_Bit_l();    break; 
       case 0x7E:   opcode_0xCB_Bit_hl();   break; 
       case 0x7F:   opcode_0xCB_Bit_a();    break; 
       case 0x80:   opcode_0xCB_Res_b();    break; 
       case 0x81:   opcode_0xCB_Res_c();    break; 
       case 0x82:   opcode_0xCB_Res_d();    break; 
       case 0x83:   opcode_0xCB_Res_e();    break; 
       case 0x84:   opcode_0xCB_Res_h();    break; 
       case 0x85:   opcode_0xCB_Res_l();    break; 
       case 0x86:   opcode_0xCB_Res_hl();   break; 
       case 0x87:   opcode_0xCB_Res_a();    break; 
       case 0x88:   opcode_0xCB_Res_b();    break; 
       case 0x89:   opcode_0xCB_Res_c();    break; 
       case 0x8A:   opcode_0xCB_Res_d();    break; 
       case 0x8B:   opcode_0xCB_Res_e();    break; 
       case 0x8C:   opcode_0xCB_Res_h();    break; 
       case 0x8D:   opcode_0xCB_Res_l();    break; 
       case 0x8E:   opcode_0xCB_Res_hl();   break; 
       case 0x8F:   opcode_0xCB_Res_a();    break; 
       case 0x90:   opcode_0xCB_Res_b();    break; 
       case 0x91:   opcode_0xCB_Res_c();    break; 
       case 0x92:   opcode_0xCB_Res_d();    break; 
       case 0x93:   opcode_0xCB_Res_e();    break; 
       case 0x94:   opcode_0xCB_Res_h();    break; 
       case 0x95:   opcode_0xCB_Res_l();    break; 
       case 0x96:   opcode_0xCB_Res_hl();   break; 
       case 0x97:   opcode_0xCB_Res_a();    break; 
       case 0x98:   opcode_0xCB_Res_b();    break; 
       case 0x99:   opcode_0xCB_Res_c();    break; 
       case 0x9A:   opcode_0xCB_Res_d();    break; 
       case 0x9B:   opcode_0xCB_Res_e();    break; 
       case 0x9C:   opcode_0xCB_Res_h();    break; 
       case 0x9D:   opcode_0xCB_Res_l();    break; 
       case 0x9E:   opcode_0xCB_Res_hl();   break; 
       case 0x9F:   opcode_0xCB_Res_a();    break; 
       case 0xA0:   opcode_0xCB_Res_b();    break; 
       case 0xA1:   opcode_0xCB_Res_c();    break; 
       case 0xA2:   opcode_0xCB_Res_d();    break; 
       case 0xA3:   opcode_0xCB_Res_e();    break; 
       case 0xA4:   opcode_0xCB_Res_h();    break; 
       case 0xA5:   opcode_0xCB_Res_l();    break; 
       case 0xA6:   opcode_0xCB_Res_hl();   break; 
       case 0xA7:   opcode_0xCB_Res_a();    break; 
       case 0xA8:   opcode_0xCB_Res_b();    break; 
       case 0xA9:   opcode_0xCB_Res_c();    break; 
       case 0xAA:   opcode_0xCB_Res_d();    break; 
       case 0xAB:   opcode_0xCB_Res_e();    break; 
       case 0xAC:   opcode_0xCB_Res_h();    break; 
       case 0xAD:   opcode_0xCB_Res_l();    break; 
       case 0xAE:   opcode_0xCB_Res_hl();   break; 
       case 0xAF:   opcode_0xCB_Res_a();    break; 
       case 0xB0:   opcode_0xCB_Res_b();    break; 
       case 0xB1:   opcode_0xCB_Res_c();    break; 
       case 0xB2:   opcode_0xCB_Res_d();    break; 
       case 0xB3:   opcode_0xCB_Res_e();    break; 
       case 0xB4:   opcode_0xCB_Res_h();    break; 
       case 0xB5:   opcode_0xCB_Res_l();    break; 
       case 0xB6:   opcode_0xCB_Res_hl();   break; 
       case 0xB7:   opcode_0xCB_Res_a();    break; 
       case 0xB8:   opcode_0xCB_Res_b();    break; 
       case 0xB9:   opcode_0xCB_Res_c();    break; 
       case 0xBA:   opcode_0xCB_Res_d();    break; 
       case 0xBB:   opcode_0xCB_Res_e();    break; 
       case 0xBC:   opcode_0xCB_Res_h();    break; 
       case 0xBD:   opcode_0xCB_Res_l();    break; 
       case 0xBE:   opcode_0xCB_Res_hl();   break; 
       case 0xBF:   opcode_0xCB_Res_a();    break; 
       case 0xC0:   opcode_0xCB_Set_b();    break; 
       case 0xC1:   opcode_0xCB_Set_c();    break; 
       case 0xC2:   opcode_0xCB_Set_d();    break; 
       case 0xC3:   opcode_0xCB_Set_e();    break; 
       case 0xC4:   opcode_0xCB_Set_h();    break; 
       case 0xC5:   opcode_0xCB_Set_l();    break; 
       case 0xC6:   opcode_0xCB_Set_hl();   break; 
       case 0xC7:   opcode_0xCB_Set_a();    break; 
       case 0xC8:   opcode_0xCB_Set_b();    break; 
       case 0xC9:   opcode_0xCB_Set_c();    break; 
       case 0xCA:   opcode_0xCB_Set_d();    break; 
       case 0xCB:   opcode_0xCB_Set_e();    break; 
       case 0xCC:   opcode_0xCB_Set_h();    break; 
       case 0xCD:   opcode_0xCB_Set_l();    break; 
       case 0xCE:   opcode_0xCB_Set_hl();   break; 
       case 0xCF:   opcode_0xCB_Set_a();    break; 
       case 0xD0:   opcode_0xCB_Set_b();    break; 
       case 0xD1:   opcode_0xCB_Set_c();    break; 
       case 0xD2:   opcode_0xCB_Set_d();    break; 
       case 0xD3:   opcode_0xCB_Set_e();    break; 
       case 0xD4:   opcode_0xCB_Set_h();    break; 
       case 0xD5:   opcode_0xCB_Set_l();    break; 
       case 0xD6:   opcode_0xCB_Set_hl();   break; 
       case 0xD7:   opcode_0xCB_Set_a();    break; 
       case 0xD8:   opcode_0xCB_Set_b();    break; 
       case 0xD9:   opcode_0xCB_Set_c();    break; 
       case 0xDA:   opcode_0xCB_Set_d();    break; 
       case 0xDB:   opcode_0xCB_Set_e();    break; 
       case 0xDC:   opcode_0xCB_Set_h();    break; 
       case 0xDD:   opcode_0xCB_Set_l();    break; 
       case 0xDE:   opcode_0xCB_Set_hl();   break; 
       case 0xDF:   opcode_0xCB_Set_a();    break; 
       case 0xE0:   opcode_0xCB_Set_b();    break; 
       case 0xE1:   opcode_0xCB_Set_c();    break; 
       case 0xE2:   opcode_0xCB_Set_d();    break; 
       case 0xE3:   opcode_0xCB_Set_e();    break; 
       case 0xE4:   opcode_0xCB_Set_h();    break; 
       case 0xE5:   opcode_0xCB_Set_l();    break; 
       case 0xE6:   opcode_0xCB_Set_hl();   break; 
       case 0xE7:   opcode_0xCB_Set_a();    break; 
       case 0xE8:   opcode_0xCB_Set_b();    break; 
       case 0xE9:   opcode_0xCB_Set_c();    break; 
       case 0xEA:   opcode_0xCB_Set_d();    break; 
       case 0xEB:   opcode_0xCB_Set_e();    break; 
       case 0xEC:   opcode_0xCB_Set_h();    break; 
       case 0xED:   opcode_0xCB_Set_l();    break; 
       case 0xEE:   opcode_0xCB_Set_hl();   break; 
       case 0xEF:   opcode_0xCB_Set_a();    break; 
       case 0xF0:   opcode_0xCB_Set_b();    break; 
       case 0xF1:   opcode_0xCB_Set_c();    break; 
       case 0xF2:   opcode_0xCB_Set_d();    break; 
       case 0xF3:   opcode_0xCB_Set_e();    break; 
       case 0xF4:   opcode_0xCB_Set_h();    break; 
       case 0xF5:   opcode_0xCB_Set_l();    break; 
       case 0xF6:   opcode_0xCB_Set_hl();   break; 
       case 0xF7:   opcode_0xCB_Set_a();    break; 
       case 0xF8:   opcode_0xCB_Set_b();    break; 
       case 0xF9:   opcode_0xCB_Set_c();    break; 
       case 0xFA:   opcode_0xCB_Set_d();    break; 
       case 0xFB:   opcode_0xCB_Set_e();    break; 
       case 0xFC:   opcode_0xCB_Set_h();    break; 
       case 0xFD:   opcode_0xCB_Set_l();    break; 
       case 0xFE:   opcode_0xCB_Set_hl();   break; 
       case 0xFF:   opcode_0xCB_Set_a();    break; 
      }
      
    return;
}
      

void decode_table_0xED()  {
    
    if (nmi.Latched())  {        // NMI allowed between CB and ED opcodes.  Back-off PC and handle the NMI
        register_pc--;
        return;
    }

    opcode_byte = Fetch_opcode();
  
    clock_counter = clock_counter + Opcode_Timing_ED[opcode_byte];

    
    switch (opcode_byte)  {
        
       case 0x00:   opcode_0x00();      break; 
       case 0x01:   opcode_0x00();      break; 
       case 0x02:   opcode_0x00();      break; 
       case 0x03:   opcode_0x00();      break; 
       case 0x04:   opcode_0x00();      break; 
       case 0x05:   opcode_0x00();      break; 
       case 0x06:   opcode_0x00();      break; 
       case 0x07:   opcode_0x00();      break; 
       case 0x08:   opcode_0x00();      break; 
       case 0x09:   opcode_0x00();      break; 
       case 0x0A:   opcode_0x00();      break; 
       case 0x0B:   opcode_0x00();      break; 
       case 0x0C:   opcode_0x00();      break; 
       case 0x0D:   opcode_0x00();      break; 
       case 0x0E:   opcode_0x00();      break; 
       case 0x0F:   opcode_0x00();      break; 
       case 0x10:   opcode_0x00();      break; 
       case 0x11:   opcode_0x00();      break; 
       case 0x12:   opcode_0x00();      break; 
       case 0x13:   opcode_0x00();      break; 
       case 0x14:   opcode_0x00();      break; 
       case 0x15:   opcode_0x00();      break; 
       case 0x16:   opcode_0x00();      break; 
       case 0x17:   opcode_0x00();      break; 
       case 0x18:   opcode_0x00();      break; 
       case 0x19:   opcode_0x00();      break; 
       case 0x1A:   opcode_0x00();      break; 
       case 0x1B:   opcode_0x00();      break; 
       case 0x1C:   opcode_0x00();      break; 
       case 0x1D:   opcode_0x00();      break; 
       case 0x1E:   opcode_0x00();      break; 
       case 0x1F:   opcode_0x00();      break; 
       case 0x20:   opcode_0x00();      break; 
       case 0x21:   opcode_0x00();      break; 
       case 0x22:   opcode_0x00();      break; 
       case 0x23:   opcode_0x00();      break; 
       case 0x24:   opcode_0x00();      break; 
       case 0x25:   opcode_0x00();      break; 
       case 0x26:   opcode_0x00();      break; 
       case 0x27:   opcode_0x00();      break; 
       case 0x28:   opcode_0x00();      break; 
       case 0x29:   opcode_0x00();      break; 
       case 0x2A:   opcode_0x00();      break; 
       case 0x2B:   opcode_0x00();      break; 
       case 0x2C:   opcode_0x00();      break; 
       case 0x2D:   opcode_0x00();      break; 
       case 0x2E:   opcode_0x00();      break; 
       case 0x2F:   opcode_0x00();      break; 
       case 0x30:   opcode_0x00();      break; 
       case 0x31:   opcode_0x00();      break; 
       case 0x32:   opcode_0x00();      break; 
       case 0x33:   opcode_0x00();      break; 
       case 0x34:   opcode_0x00();      break; 
       case 0x35:   opcode_0x00();      break; 
       case 0x36:   opcode_0x00();      break; 
       case 0x37:   opcode_0x00();      break; 
       case 0x38:   opcode_0x00();      break; 
       case 0x39:   opcode_0x00();      break; 
       case 0x3A:   opcode_0x00();      break; 
       case 0x3B:   opcode_0x00();      break; 
       case 0x3C:   opcode_0x00();      break; 
       case 0x3D:   opcode_0x00();      break; 
       case 0x3E:   opcode_0x00();      break; 
       case 0x3F:   opcode_0x00();      break; 
       case 0x40:   opcode_0xED40();    break; 
       case 0x41:   opcode_0xED41();    break; 
       case 0x42:   opcode_0xED42();    break; 
       case 0x43:   opcode_0xED43();    break; 
       case 0x44:   opcode_0xED44();    break; 
       case 0x45:   opcode_0xED45();    break; 
       case 0x46:   opcode_0xED46();    break; 
       case 0x47:   opcode_0xED47();    break; 
       case 0x48:   opcode_0xED48();    break; 
       case 0x49:   opcode_0xED49();    break; 
       case 0x4A:   opcode_0xED4A();    break; 
       case 0x4B:   opcode_0xED4B();    break; 
       case 0x4C:   opcode_0xED44();    break; 
       case 0x4D:   opcode_0xED4D();    break; 
       case 0x4E:   opcode_0x00();      break; 
       case 0x4F:   opcode_0xED4F();    break; 
       case 0x50:   opcode_0xED50();    break; 
       case 0x51:   opcode_0xED51();    break; 
       case 0x52:   opcode_0xED52();    break; 
       case 0x53:   opcode_0xED53();    break; 
       case 0x54:   opcode_0xED44();    break; 
       case 0x55:   opcode_0xED45();    break; 
       case 0x56:   opcode_0xED56();    break; 
       case 0x57:   opcode_0xED57();    break; 
       case 0x58:   opcode_0xED58();    break; 
       case 0x59:   opcode_0xED59();    break; 
       case 0x5A:   opcode_0xED5A();    break; 
       case 0x5B:   opcode_0xED5B();    break; 
       case 0x5C:   opcode_0xED44();    break; 
       case 0x5D:   opcode_0xED45();    break; 
       case 0x5E:   opcode_0xED5E();    break; 
       case 0x5F:   opcode_0xED5F();    break; 
       case 0x60:   opcode_0xED60();    break; 
       case 0x61:   opcode_0xED61();    break; 
       case 0x62:   opcode_0xED62();    break; 
       case 0x63:   opcode_0xED63();    break; 
       case 0x64:   opcode_0xED44();    break; 
       case 0x65:   opcode_0xED45();    break; 
       case 0x66:   opcode_0xED46();    break; 
       case 0x67:   opcode_0xED67();    break; 
       case 0x68:   opcode_0xED68();    break; 
       case 0x69:   opcode_0xED69();    break; 
       case 0x6A:   opcode_0xED6A();    break; 
       case 0x6B:   opcode_0xED6B();    break; 
       case 0x6C:   opcode_0xED44();    break; 
       case 0x6D:   opcode_0xED45();    break; 
       case 0x6E:   opcode_0x00();      break; 
       case 0x6F:   opcode_0xED6F();    break; 
       case 0x70:   opcode_0xED70();    break; 
       case 0x71:   opcode_0xED71();    break; 
       case 0x72:   opcode_0xED72();    break; 
       case 0x73:   opcode_0xED73();    break; 
       case 0x74:   opcode_0xED44();    break; 
       case 0x75:   opcode_0xED45();    break; 
       case 0x76:   opcode_0xED56();    break; 
       case 0x77:   opcode_0x00();      break; 
       case 0x78:   opcode_0xED78();    break; 
       case 0x79:   opcode_0xED79();    break; 
       case 0x7A:   opcode_0xED7A();    break; 
       case 0x7B:   opcode_0xED7B();    break; 
       case 0x7C:   opcode_0xED44();    break; 
       case 0x7D:   opcode_0xED45();    break; 
       case 0x7E:   opcode_0xED5E();    break; 
       case 0x7F:   opcode_0x00();      break; 
       case 0x80:   opcode_0x00();      break; 
       case 0x81:   opcode_0x00();      break; 
       case 0x82:   opcode_0x00();      break; 
       case 0x83:   opcode_0x00();      break; 
       case 0x84:   opcode_0x00();      break; 
       case 0x85:   opcode_0x00();      break; 
       case 0x86:   opcode_0x00();      break; 
       case 0x87:   opcode_0x00();      break; 
       case 0x88:   opcode_0x00();      break; 
       case 0x89:   opcode_0x00();      break; 
       case 0x8A:   opcode_0x00();      break; 
       case 0x8B:   opcode_0x00();      break; 
       case 0x8C:   opcode_0x00();      break; 
       case 0x8D:   opcode_0x00();      break; 
       case 0x8E:   opcode_0x00();      break; 
       case 0x8F:   opcode_0x00();      break; 
       case 0x90:   opcode_0x00();      break;   
       case 0x91:   opcode_0x00();      break;  
       case 0x92:   opcode_0x00();      break;  
       case 0x93:   opcode_0x00();      break;  
       case 0x94:   opcode_0x00();      break;  
       case 0x95:   opcode_0x00();      break;  
       case 0x96:   opcode_0x00();      break;  
       case 0x97:   opcode_0x00();      break;  
       case 0x98:   opcode_0x00();      break; 
       case 0x99:   opcode_0x00();      break; 
       case 0x9A:   opcode_0x00();      break; 
       case 0x9B:   opcode_0x00();      break; 
       case 0x9C:   opcode_0x00();      break; 
       case 0x9D:   opcode_0x00();      break; 
       case 0x9E:   opcode_0x00();      break; 
       case 0x9F:   opcode_0x00();      break; 
       case 0xA0:   opcode_0xEDA0();    break; 
       case 0xA1:   opcode_0xEDA1();    break; 
       case 0xA2:   opcode_0xEDA2();    break; 
       case 0xA3:   opcode_0xEDA3();    break; 
       case 0xA4:   opcode_0x00();      break; 
       case 0xA5:   opcode_0x00();      break; 
       case 0xA6:   opcode_0x00();      break; 
       case 0xA7:   opcode_0x00();      break; 
       case 0xA8:   opcode_0xEDA8();    break; 
       case 0xA9:   opcode_0xEDA9();    break; 
       case 0xAA:   opcode_0xEDAA();    break; 
       case 0xAB:   opcode_0xEDAB();    break; 
       case 0xAC:   opcode_0x00();      break; 
       case 0xAD:   opcode_0x00();      break; 
       case 0xAE:   opcode_0x00();      break; 
       case 0xAF:   opcode_0x00();      break; 
       case 0xB0:   opcode_0xEDA0();    break; 
       case 0xB1:   opcode_0xEDA1();    break; 
       case 0xB2:   opcode_0xEDA2();    break; 
       case 0xB3:   opcode_0xEDA3();    break; 
       case 0xB4:   opcode_0x00();      break; 
       case 0xB5:   opcode_0x00();      break; 
       case 0xB6:   opcode_0x00();      break; 
       case 0xB7:   opcode_0x00();      break; 
       case 0xB8:   opcode_0xEDA8();    break; 
       case 0xB9:   opcode_0xEDA9();    break; 
       case 0xBA:   opcode_0xEDAA();    break; 
       case 0xBB:   opcode_0xEDAB();    break; 
       case 0xBC:   opcode_0x00();      break; 
       case 0xBD:   opcode_0x00();      break; 
       case 0xBE:   opcode_0x00();      break; 
       case 0xBF:   opcode_0x00();      break; 
       case 0xC0:   opcode_0x00();      break; 
       case 0xC1:   opcode_0x00();      break; 
       case 0xC2:   opcode_0x00();      break; 
       case 0xC3:   opcode_0x00();      break; 
       case 0xC4:   opcode_0x00();      break; 
       case 0xC5:   opcode_0x00();      break; 
       case 0xC6:   opcode_0x00();      break; 
       case 0xC7:   opcode_0x00();      break; 
       case 0xC8:   opcode_0x00();      break; 
       case 0xC9:   opcode_0x00();      break; 
       case 0xCA:   opcode_0x00();      break; 
       case 0xCB:   opcode_0x00();      break; 
       case 0xCC:   opcode_0x00();      break; 
       case 0xCD:   opcode_0x00();      break; 
       case 0xCE:   opcode_0x00();      break; 
       case 0xCF:   opcode_0x00();      break; 
       case 0xD0:   opcode_0x00();      break; 
       case 0xD1:   opcode_0x00();      break; 
       case 0xD2:   opcode_0x00();      break; 
       case 0xD3:   opcode_0x00();      break; 
       case 0xD4:   opcode_0x00();      break; 
       case 0xD5:   opcode_0x00();      break; 
       case 0xD6:   opcode_0x00();      break; 
       case 0xD7:   opcode_0x00();      break; 
       case 0xD8:   opcode_0x00();      break; 
       case 0xD9:   opcode_0x00();      break; 
       case 0xDA:   opcode_0x00();      break; 
       case 0xDB:   opcode_0x00();      break; 
       case 0xDC:   opcode_0x00();      break; 
       case 0xDD:   opcode_0x00();      break; 
       case 0xDE:   opcode_0x00();      break; 
       case 0xDF:   opcode_0x00();      break; 
       case 0xE0:   opcode_0x00();      break; 
       case 0xE1:   opcode_0x00();      break; 
       case 0xE2:   opcode_0x00();      break; 
       case 0xE3:   opcode_0x00();      break; 
       case 0xE4:   opcode_0x00();      break; 
       case 0xE5:   opcode_0x00();      break; 
       case 0xE6:   opcode_0x00();      break; 
       case 0xE7:   opcode_0x00();      break; 
       case 0xE8:   opcode_0x00();      break; 
       case 0xE9:   opcode_0x00();      break; 
       case 0xEA:   opcode_0x00();      break; 
       case 0xEB:   opcode_0x00();      break; 
       case 0xEC:   opcode_0x00();      break; 
       case 0xED:   opcode_0x00();      break; 
       case 0xEE:   opcode_0x00();      break; 
       case 0xEF:   opcode_0x00();      break; 
       case 0xF0:   opcode_0x00();      break; 
       case 0xF1:   opcode_0x00();      break; 
       case 0xF2:   opcode_0x00();      break; 
       case 0xF3:   opcode_0x00();      break; 
       case 0xF4:   opcode_0x00();      break; 
       case 0xF5:   opcode_0x00();      break;   
       case 0xF6:   opcode_0x00();      break;  
       case 0xF7:   opcode_0x00();      break;  
       case 0xF8:   opcode_0x00();      break;   
       case 0xF9:   opcode_0x00();      break;   
       case 0xFA:   opcode_0x00();      break;  
       case 0xFB:   opcode_0x00();      break;  
       case 0xFC:   opcode_0x00();      break;   
       case 0xFD:   opcode_0x00();      break;   
       case 0xFE:   opcode_0x00();      break;   
       case 0xFF:   opcode_0x00();      break;   
      }
      
    return;
}
      

// ------------------------------------------------------------------------------------------------------------
// Decode the first byte of the opcode
// ------------------------------------------------------------------------------------------------------------
void execute_instruction()  {
    
    opcode_byte = Fetch_opcode(); 
    clock_counter=Opcode_Timing_Main[opcode_byte];

    switch (opcode_byte)  {
        
       case 0x00:   opcode_0x00();    break; 
       case 0x01:   opcode_0x01();    break; 
       case 0x02:   opcode_0x02();    break; 
       case 0x03:   opcode_0x03();    break; 
       case 0x04:   opcode_0x04();    break; 
       case 0x05:   opcode_0x05();    break; 
       case 0x06:   opcode_0x06();    break; 
       case 0x07:   opcode_0x07();    break; 
       case 0x08:   opcode_0x08();    break; 
       case 0x09:   opcode_0x09();    break; 
       case 0x0A:   opcode_0x0A();    break; 
       case 0x0B:   opcode_0x0B();    break; 
       case 0x0C:   opcode_0x0C();    break; 
       case 0x0D:   opcode_0x0D();    break; 
       case 0x0E:   opcode_0x0E();    break; 
       case 0x0F:   opcode_0x0F();    break; 
       case 0x10:   opcode_0x10();    break; 
       case 0x11:   opcode_0x11();    break; 
       case 0x12:   opcode_0x12();    break; 
       case 0x13:   opcode_0x13();    break; 
       case 0x14:   opcode_0x14();    break; 
       case 0x15:   opcode_0x15();    break; 
       case 0x16:   opcode_0x16();    break; 
       case 0x17:   opcode_0x17();    break; 
       case 0x18:   opcode_0x18();    break; 
       case 0x19:   opcode_0x19();    break; 
       case 0x1A:   opcode_0x1A();    break; 
       case 0x1B:   opcode_0x1B();    break; 
       case 0x1C:   opcode_0x1C();    break; 
       case 0x1D:   opcode_0x1D();    break; 
       case 0x1E:   opcode_0x1E();    break; 
       case 0x1F:   opcode_0x1F();    break; 
       case 0x20:   opcode_0x20();    break; 
       case 0x21:   opcode_0x21();    break; 
       case 0x22:   opcode_0x22();    break; 
       case 0x23:   opcode_0x23();    break; 
       case 0x24:   opcode_0x24();    break; 
       case 0x25:   opcode_0x25();    break; 
       case 0x26:   opcode_0x26();    break; 
       case 0x27:   opcode_0x27();    break; 
       case 0x28:   opcode_0x28();    break; 
       case 0x29:   opcode_0x29();    break; 
       case 0x2A:   opcode_0x2A();    break; 
       case 0x2B:   opcode_0x2B();    break; 
       case 0x2C:   opcode_0x2C();    break; 
       case 0x2D:   opcode_0x2D();    break; 
       case 0x2E:   opcode_0x2E();    break; 
       case 0x2F:   opcode_0x2F();    break; 
       case 0x30:   opcode_0x30();    break; 
       case 0x31:   opcode_0x31();    break; 
       case 0x32:   opcode_0x32();    break; 
       case 0x33:   opcode_0x33();    break; 
       case 0x34:   opcode_0x34();    break; 
       case 0x35:   opcode_0x35();    break; 
       case 0x36:   opcode_0x36();    break; 
       case 0x37:   opcode_0x37();    break; 
       case 0x38:   opcode_0x38();    break; 
       case 0x39:   opcode_0x39();    break; 
       case 0x3a:   opcode_0x3A();    break; 
       case 0x3B:   opcode_0x3B();    break; 
       case 0x3C:   opcode_0x3C();    break; 
       case 0x3D:   opcode_0x3D();    break; 
       case 0x3E:   opcode_0x3E();    break; 
       case 0x3F:   opcode_0x3F();    break; 
       case 0x40:   opcode_0x40();    break; 
       case 0x41:   opcode_0x41();    break; 
       case 0x42:   opcode_0x42();    break; 
       case 0x43:   opcode_0x43();    break; 
       case 0x44:   opcode_0x44();    break; 
       case 0x45:   opcode_0x45();    break; 
       case 0x46:   opcode_0x46();    break; 
       case 0x47:   opcode_0x47();    break; 
       case 0x48:   opcode_0x48();    break; 
       case 0x49:   opcode_0x49();    break; 
       case 0x4A:   opcode_0x4A();    break; 
       case 0x4B:   opcode_0x4B();    break; 
       case 0x4C:   opcode_0x4C();    break; 
       case 0x4D:   opcode_0x4D();    break; 
       case 0x4E:   opcode_0x4E();    break; 
       case 0x4F:   opcode_0x4F();    break; 
       case 0x50:   opcode_0x50();    break; 
       case 0x51:   opcode_0x51();    break; 
       case 0x52:   opcode_0x52();    break; 
       case 0x53:   opcode_0x53();    break; 
       case 0x54:   opcode_0x54();    break; 
       case 0x55:   opcode_0x55();    break; 
       case 0x56:   opcode_0x56();    break; 
       case 0x57:   opcode_0x57();    break; 
       case 0x58:   opcode_0x58();    break; 
       case 0x59:   opcode_0x59();    break; 
       case 0x5A:   opcode_0x5A();    break; 
       case 0x5B:   opcode_0x5B();    break; 
       case 0x5C:   opcode_0x5C();    break; 
       case 0x5D:   opcode_0x5D();    break; 
       case 0x5E:   opcode_0x5E();    break; 
       case 0x5F:   opcode_0x5F();    break; 
       case 0x60:   opcode_0x60();    break; 
       case 0x61:   opcode_0x61();    break; 
       case 0x62:   opcode_0x62();    break; 
       case 0x63:   opcode_0x63();    break; 
       case 0x64:   opcode_0x64();    break; 
       case 0x65:   opcode_0x65();    break; 
       case 0x66:   opcode_0x66();    break; 
       case 0x67:   opcode_0x67();    break; 
       case 0x68:   opcode_0x68();    break; 
       case 0x69:   opcode_0x69();    break; 
       case 0x6A:   opcode_0x6A();    break; 
       case 0x6B:   opcode_0x6B();    break; 
       case 0x6C:   opcode_0x6C();    break; 
       case 0x6D:   opcode_0x6D();    break; 
       case 0x6E:   opcode_0x6E();    break; 
       case 0x6F:   opcode_0x6F();    break; 
       case 0x70:   opcode_0x70();    break; 
       case 0x71:   opcode_0x71();    break; 
       case 0x72:   opcode_0x72();    break; 
       case 0x73:   opcode_0x73();    break; 
       case 0x74:   opcode_0x74();    break; 
       case 0x75:   opcode_0x75();    break; 
       case 0x76:   opcode_0x76();    break; 
       case 0x77:   opcode_0x77();    break; 
       case 0x78:   opcode_0x78();    break; 
       case 0x79:   opcode_0x79();    break; 
       case 0x7A:   opcode_0x7A();    break; 
       case 0x7B:   opcode_0x7B();    break; 
       case 0x7C:   opcode_0x7C();    break; 
       case 0x7D:   opcode_0x7D();    break; 
       case 0x7E:   opcode_0x7E();    break; 
       case 0x7F:   opcode_0x7F();    break; 
       case 0x80:   opcode_0x80();    break; 
       case 0x81:   opcode_0x81();    break; 
       case 0x82:   opcode_0x82();    break; 
       case 0x83:   opcode_0x83();    break; 
       case 0x84:   opcode_0x84();    break; 
       case 0x85:   opcode_0x85();    break; 
       case 0x86:   opcode_0x86();    break; 
       case 0x87:   opcode_0x87();    break; 
       case 0x88:   opcode_0x88();    break; 
       case 0x89:   opcode_0x89();    break; 
       case 0x8A:   opcode_0x8A();    break; 
       case 0x8B:   opcode_0x8B();    break; 
       case 0x8C:   opcode_0x8C();    break; 
       case 0x8D:   opcode_0x8D();    break; 
       case 0x8E:   opcode_0x8E();    break; 
       case 0x8F:   opcode_0x8F();    break; 
       case 0x90:   opcode_0x90();    break; 
       case 0x91:   opcode_0x91();    break; 
       case 0x92:   opcode_0x92();    break; 
       case 0x93:   opcode_0x93();    break; 
       case 0x94:   opcode_0x94();    break; 
       case 0x95:   opcode_0x95();    break; 
       case 0x96:   opcode_0x96();    break; 
       case 0x97:   opcode_0x97();    break; 
       case 0x98:   opcode_0x98();    break; 
       case 0x99:   opcode_0x99();    break; 
       case 0x9A:   opcode_0x9A();    break; 
       case 0x9B:   opcode_0x9B();    break; 
       case 0x9C:   opcode_0x9C();    break; 
       case 0x9D:   opcode_0x9D();    break; 
       case 0x9E:   opcode_0x9E();    break; 
       case 0x9F:   opcode_0x9F();    break; 
       case 0xA0:   opcode_0xA0();    break; 
       case 0xA1:   opcode_0xA1();    break; 
       case 0xA2:   opcode_0xA2();    break; 
       case 0xA3:   opcode_0xA3();    break; 
       case 0xA4:   opcode_0xA4();    break; 
       case 0xA5:   opcode_0xA5();    break; 
       case 0xA6:   opcode_0xA6();    break; 
       case 0xA7:   opcode_0xA7();    break;        
       case 0xA8:   opcode_0xA8();    break; 
       case 0xA9:   opcode_0xA9();    break; 
       case 0xAA:   opcode_0xAA();    break; 
       case 0xAB:   opcode_0xAB();    break; 
       case 0xAC:   opcode_0xAC();    break; 
       case 0xAD:   opcode_0xAD();    break; 
       case 0xAE:   opcode_0xAE();    break; 
       case 0xAF:   opcode_0xAF();    break; 
       case 0xB0:   opcode_0xB0();    break; 
       case 0xB1:   opcode_0xB1();    break; 
       case 0xB2:   opcode_0xB2();    break; 
       case 0xB3:   opcode_0xB3();    break; 
       case 0xB4:   opcode_0xB4();    break; 
       case 0xB5:   opcode_0xB5();    break; 
       case 0xB6:   opcode_0xB6();    break; 
       case 0xB7:   opcode_0xB7();    break; 
       case 0xB8:   opcode_0xB8();    break; 
       case 0xB9:   opcode_0xB9();    break; 
       case 0xBA:   opcode_0xBA();    break; 
       case 0xBB:   opcode_0xBB();    break; 
       case 0xBC:   opcode_0xBC();    break; 
       case 0xBD:   opcode_0xBD();    break; 
       case 0xBE:   opcode_0xBE();    break; 
       case 0xBF:   opcode_0xBF();    break; 
       case 0xC0:   opcode_0xC0();    break; 
       case 0xC1:   opcode_0xC1();    break; 
       case 0xC2:   opcode_0xC2();    break; 
       case 0xC3:   opcode_0xC3();    break; 
       case 0xC4:   opcode_0xC4();    break; 
       case 0xC5:   opcode_0xC5();    break; 
       case 0xC6:   opcode_0xC6();    break; 
       case 0xC7:   opcode_0xC7();    break; 
       case 0xC8:   opcode_0xC8();    break; 
       case 0xC9:   opcode_0xC9();    break; 
       case 0xCA:   opcode_0xCA();    break; 
       case 0xCB:   decode_table_0xCB();    break; 
       case 0xCC:   opcode_0xCC();    break; 
       case 0xCD:   opcode_0xCD();    break; 
       case 0xCE:   opcode_0xCE();    break; 
       case 0xCF:   opcode_0xCF();    break; 
       case 0xD0:   opcode_0xD0();    break; 
       case 0xD1:   opcode_0xD1();    break; 
       case 0xD2:   opcode_0xD2();    break; 
       case 0xD3:   opcode_0xD3();    break; 
       case 0xD4:   opcode_0xD4();    break; 
       case 0xD5:   opcode_0xD5();    break; 
       case 0xD6:   opcode_0xD6();    break; 
       case 0xD7:   opcode_0xD7();    break; 
       case 0xD8:   opcode_0xD8();    break; 
       case 0xD9:   opcode_0xD9();    break; 
       case 0xDA:   opcode_0xDA();    break; 
       case 0xDB:   opcode_0xDB();    break; 
       case 0xDC:   opcode_0xDC();    break; 
       case 0xDD:   opcode_0xDD();    break; 
       case 0xDE:   opcode_0xDE();    break; 
       case 0xDF:   opcode_0xDF();    break; 
       case 0xE0:   opcode_0xE0();    break; 
       case 0xE1:   opcode_0xE1();    break; 
       case 0xE2:   opcode_0xE2();    break; 
       case 0xE3:   opcode_0xE3();    break; 
       case 0xE4:   opcode_0xE4();    break; 
       case 0xE5:   opcode_0xE5();    break; 
       case 0xE6:   opcode_0xE6();    break;        
       case 0xE7:   opcode_0xE7();    break; 
       case 0xE8:   opcode_0xE8();    break; 
       case 0xE9:   opcode_0xE9();    break; 
       case 0xEA:   opcode_0xEA();    break; 
       case 0xEB:   opcode_0xEB();    break; 
       case 0xEC:   opcode_0xEC();    break; 
       case 0xED:   decode_table_0xED();    break; 
       case 0xEE:   opcode_0xEE();    break; 
       case 0xEF:   opcode_0xEF();    break; 
       case 0xF0:   opcode_0xF0();    break; 
       case 0xF1:   opcode_0xF1();    break; 
       case 0xF2:   opcode_0xF2();    break; 
       case 0xF3:   opcode_0xF3();    break; 
       case 0xF4:   opcode_0xF4();    break; 
       case 0xF5:   opcode_0xF5();    break; 
       case 0xF6:   opcode_0xF6();    break; 
       case 0xF7:   opcode_0xF7();    break; 
       case 0xF8:   opcode_0xF8();    break; 
       case 0xF9:   opcode_0xF9();    break; 
       case 0xFA:   opcode_0xFA();    break; 
       case 0xFB:   opcode_0xFB();    break; 
       case 0xFC:   opcode_0xFC();    break; 
       case 0xFD:   opcode_0xFD();    break;   
       case 0xFE:   opcode_0xFE();    break; 
       case 0xFF:   opcode_0xFF();    break; 
      }
      
return;
}



// -------------------------------------------------
//
// Main loop
//
// -------------------------------------------------
 void loop() {
  uint16_t local_counter=0;


  // Give Teensy 4.1 a moment
  delay (1000);
  wait_for_CLK_rising_edge();
  wait_for_CLK_rising_edge();
  wait_for_CLK_rising_edge();

  reset_sequence();
  
  // Copy the motherboard ROMS into internal RAM for acceleration
  //
  for (local_counter=0; local_counter<=0x37DF; local_counter++)  {
    internal_RAM[local_counter] = BIU_Bus_Cycle(MEM_READ_BYTE , local_counter , 0x00 );
  }

  local_counter=0;
  
  while (true) {
                
      if (gpio6.ResetAsserted()) {
          reset_sequence();
      }

      // Set Acceleration using UART receive characters
      // Send the numbers 0,1,2,3 from the host through a serial terminal to the MCL65+
      // for acceleration modes 0,1,2,3
      //
      local_counter++;
      if (local_counter==8000){
        if (Serial.available() ) { 
          incomingByte = Serial.read();   
          switch (incomingByte){
            case 48: mode=0;  Serial.println("M0"); break;
            case 49: mode=1;  Serial.println("M1"); break;
            case 50: mode=2;  Serial.println("M2"); break;
            case 51: mode=3;  Serial.println("M3"); break;
          }
        }
      }
      
      
      // Poll for interrupts between instructions, 
      // but not when immediately after instruction enabling interrupts,
      // or if the last opcode was a prefix.
      //
      if (last_instruction_set_a_prefix==0 && pause_interrupts==0)  {
        if (nmi.Latched()) {
            NMI_Handler();
        } else if (intr.Latched() && register_iff1==1) {
            INTR_Handler();
        }
      }
      pause_interrupts=0;                // debounce
      last_instruction_set_a_prefix=0;


      // Process new instruction
      //
      execute_instruction();    
      
      if (last_instruction_set_a_prefix==0) {
          prefix_dd = 0;
          prefix_fd = 0;
          prefix_cb = 0;
      }

      // Wait for cycle counter to reach zero before processing interrupts or the next instruction
      //
      if (mode<3) while (clock_counter>0)  {  wait_for_CLK_falling_edge();   }

          
// ** End main loop



  }
      
 } 
