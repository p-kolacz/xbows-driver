// Control custom layer

// We need to convert a keyboard program to a sequence of packets.  This code
// is similar but not identical to the driver layer program.

#include <endian.h>
#include <cassert>
#include <cmath>
#include <vector>

#include "xbows.hh"
#include "custom_layer.hh"
#include "keymap.hh"

using namespace std;

uint16_t& addr_to_16(unsigned char* addr) { return *(uint16_t*)addr; }
uint32_t& addr_to_32(unsigned char* addr) { return *(uint32_t*)addr; }


// For custom layer commands, the subcommand indicates the layer 0x01, 0x02,
// 0x03 are custom layers 1-3.
//
// Handles rebinding and disabling keys.
//
vector<packet> custom_keymap_program(int layer, keymap& kmap) {
  if (layer < 1 || layer > 3)
    throw runtime_error("Bad layer");
  
  vector<packet> program;

  // Light program.  14 key rgb per packet
  int remaining = kmap.size();
  for (int i=0; i < kmap.size(); i+=14, remaining-=14) {
    packet pkt(0x22, layer);	// This is different from driver_keymap prog

    // Number of bytes already added to the program
    pkt.progcount = htole16(i * 4);

    // Store bytes of packet data in byte 5
    unsigned char pktbytes = min(remaining * 4, 56);
    pkt.nil = pktbytes;		// XXX NEED BETTER HANDLING OF THIS

    // Copy the packet data
    for (int j=0; j < min(remaining, 14); j++) {
      uint32_t val = htole32(kmap.keys[i+j]);
      memcpy(pkt.data+j*4, &val, 4);
    }
    program.push_back(pkt);
    // cout << "push packet " << pkt.to_string();
  }
  
  return program;
}

// XXX empty macros for now
vector<packet> custom_macro_program(int layer) {
  if (layer < 1 || layer > 3)
    throw runtime_error("Bad layer");

  return vector<packet>();
}


// XXX for now we have a default inactive flashlight sequence
// It seems there must be 3 packets of flashlight even when nothing is
// programmed.  XXX Is this true?
vector<packet> custom_flashlight_program(int layer, program& prog) {
  if (layer < 1 || layer > 3)
    throw runtime_error("Bad layer");
  
  vector<packet> program;

  packet pkt1(0x26, layer);
  memcpy(pkt1.data, prog.flashlight_keys, 56);
  pkt1.nil = 0x38;		// Datasize is reversed here
  program.push_back(pkt1);

  packet pkt2(0x26, layer);
  memcpy(pkt2.data, prog.flashlight_keys + 56, 56);
  pkt2.progcount = htole16(0x38);
  pkt2.nil = 0x38;		// Datasize is reversed here
  program.push_back(pkt2);

  packet pkt3(0x26, layer);
  memcpy(pkt3.data, prog.flashlight_keys + 112, 8);
  pkt3.progcount = htole16(0x70);
  pkt3.nil = 0x8;		// Datasize is reversed here
  program.push_back(pkt3);
  
  return program;
}

// Constructor
animation_frame::animation_frame() {
  header = htole32(0x00160003);
  clear();
}


// Map from keycode to bit position in animation frame.  Each key gets
// assigned to a byte and a bit within the byte.  The least significant nibble
// is the bit position.  The most significant nibbles are the byte position.
// int is overkill but we need more than 8 bits to represent the byte position
// because the bitmap is 22 bytes long.
//
// 0xff is unassigned.
int animation_assign[MAX_KEYCODE] = {
  // Position 0 is empty
  0xff,				  // K_NONE
  // Letters
  0x83, 0xb6, 0xb4, 0x86,			  // A B C D 
  0x60, 0x87, 0x90, 0x92, 0x66, 0x93, 0x94, 0x96, // E F G H I J K L
  0xc1, 0xc0, 0x70, 0x71, 0x55, 0x61,		  // M N O P Q R
  0x84, 0x62, 0x65, 0xb5, 0x56, 0xb2, 0x64, 0xb1,	  // S T U V W X Y Z
  // Numbers
  0x27, 0x30, 0x32, 0x33, 0x34, 0x36,	    // 1 2 3 4 5 6 
  0x37, 0x40, 0x42, 0x43,		    // 7 8 9 0 
  // Other printing chars
  0x44, 0x45, 0x72, 0x73, 0x74,		    // -_ += [{ ]} \|
  0x97, 0xa0, 0xc2,			    // ;: '" ,<
  0xc4, 0xc5, 0x26,			    // .> /? `~
  // Nonprinting keys
  0x00, 0x54, 0x82,			    // Esc Tab Capslock
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x10, // F1 F2 F3 F4 F5 F6 F7
  0x11, 0x12, 0x13, 0x14, 0x15,		    // F8 F9 F10 F11 F12
  0xa1, 0xb7,		      // REnter MEnter
  0xb0, 0xe6, 0xc6,	      // LShift MShift RShift
  0xd6, 0xe4, 0xf4,	      // LControl MControl RControl
  0xe0, 0xf0,		      // LAlt RAlt
  0x91, 0x46,		      // MBackspace RBackspace
  0xe3, 0xe7,		      // LSpace RSpace
  0xd7,			      // Windows
  0x75, 0xa3,		      // PageUp PageDown
  0xf6, 0xf7, 0xd0, 0xf6,     // Left Right Up Down
  0x20, 0x16,		      // PrtScrn Delete
  0x63,			      // XBows
  // Numpad
  0xff, 0xff, 0xff, 0xff, 	// Numlock NPSlash NPStar NPEnter
  0xff, 0xff, 0xff, 0xff,	// NP1 NP2 NP3 NP4
  0xff, 0xff, 0xff, 0xff,	// NP5 NP6 NP7 NP8
  0xff, 0xff, 0xff, 0xff, 0xff,	// NP9 NP0 NP. NP- NP+
  // Media
  0xff, 0xff, 0xff, 0xff,	// Play Pause Stop Last
  0xff, 0xff, 0xff, 0xff,	// Next VolUp VolDown Mute
  // Mouse
  0xff, 0xff, 0xff, 0xff, 0xff,	// LClick MClick RClick Back Forward
  // Sys/net
  0xff, 0xff, 0xff, 0xff,	// NetBack NetFwd NetRefresh NetCollection
  0xff, 0xff, 0xff, 0xff,	// NetHome NetEmail NetComp NetCalc
  0xff, 0xff, 0xff,		// NetCopy NetPaste NetPrtScrn
  // Not on xbows kbd
  0xff, 0xff, 0xff,		// Home End Insert
  // Function key
  0xf3				// Fn
};

// Enable the bit associated with key in keymap.  keymap is a 22 char bitmap.
// Same for both animation and light frame keymaps.
void enable_key(uint8_t* keymap, keycodes key) {
  int code = animation_assign[key];
  int byte = code >> 4;                // byte position is everything but lowest 4 bits
  int bit = code & 0xf;                // Low 4 bits is the bit to enable
  int mask = 1 << bit;
  keymap[byte] |= mask;
}


// Enable the bit associated with key in keymap.  keymap is a 22 char bitmap.
void animation_frame::enable(keycodes key) { enable_key(keymap, key); }

// Clear the bitmap
void animation_frame::clear() { memset(keymap, 0, 22); }

// Enable the bit associated with key in keymap.  keymap is a 22 char bitmap.
void pattern_frame::enable(keycodes key) { enable_key(keymap, key); }

// Clear the bitmap
void pattern_frame::clear() { memset(keymap, 0, 22); }


// Pack bytes into program, adding more packets as needed.
vector<packet>& pack_data(vector<packet>& program, unsigned char* data, int count) {
  // Assumes there is a packet at the end of program to look at
  assert(program.size());
  while (count) {
    // Add new packet if needed.
    if (program.back().datasize == 56) {
      packet& pkt = program.back();
      program.push_back(packet(pkt.cmd, pkt.sub));

      // Update program bytes preceding this packet
      uint16_t progcount = le16toh(pkt.progcount) + 56;
      program.back().progcount = progcount;
    }
    

    // // Copy command info forward
    // if (program.size() > 1) {
    //   unsigned psz = program.size()-1;
    //   program[psz].cmd = program[psz-1].cmd;
    //   program[psz].sub = program[psz-1].sub;
    //   program[psz].progcount = htole16(le16toh(program[psz-1].progcount) + 56);
    // }
    
    // Try to fill the current packet
    packet& pkt = program.back();

    // Pack up to 56 bytes of data into packet
    int pcount = min(count, 56-pkt.datasize);
    memcpy(pkt.data + pkt.datasize, data, pcount);
    pkt.datasize += pcount;
    
    count -= pcount;
    data += pcount;
  }
  return program;
}


// Store a single custom light program into packets
void custom_light_program(vector<packet>& program,
			  custom_light_prog& frames) {
  // Add animation frames
  for (auto f: frames.aframes)
    pack_data(program, f.data, 26);

  // Add light frames
  for (auto f: frames.lframes)
    pack_data(program, f.data, 32);
}


// Multiple lighting programs can actually be set up.  First is for the
// regular custom light.  The others are flashlight programs.  They all get
// assembled into packets here.
vector<packet> custom_light_programs(int layer, program& prog) {
  if (layer < 2 || layer > 4)
    throw runtime_error("Bad layer");

  // Initialize packet sequence with 1 empty packet.
  vector<packet> packets(1, packet(0x27, (unsigned char)layer));

  // Construct frame info bytes
  int infosize = prog.flashlights.size() + 1; // +1 for custom_lights
  infosize *= 16;			 // bytes consumed by frame info
  unsigned char info[infosize];
  uint32_t* info_p = (uint32_t*)info;

  // Frame info for custom lights
  int anim_ct = prog.custom_lights.aframes.size();
  int lite_ct = prog.custom_lights.lframes.size();

  unsigned int framestart = 0x0200; // Start of anim frames for custom light.
  *info_p++ = htole32(framestart);
  *info_p++ = htole32(anim_ct);
  framestart += anim_ct * 0x1a;	// start of light frames
  *info_p++ = htole32(framestart);
  *info_p++ = htole32(lite_ct);
  framestart += lite_ct * 0x20;	// start of light frames

  // Frame info for flashlights
  for (unsigned i=0; i < prog.flashlights.size(); i++) {
    // Frame info for custom lights
    int anim_ct = prog.flashlights[i].aframes.size();
    int lite_ct = prog.flashlights[i].lframes.size();

    *info_p++ = htole32(framestart);
    *info_p++ = htole32(anim_ct);
    framestart += anim_ct * 0x1a;	// start of light frames
    *info_p++ = htole32(framestart);
    *info_p++ = htole32(lite_ct);
    framestart += lite_ct * 0x20;	// start of light frames
  }

  // Pack frame info
  pack_data(packets, info, infosize);

  
  // Fill remaining space before lighting programs
  if (infosize > 0x200)
    throw runtime_error("No more than 128 lighting programs can be specified");
  int fillbytes = 0x200 - infosize;

  // Fill in 124 ints of 0xffffffff into last packet, 8 full packets, and
  // start of another packet.
  unsigned char ffs[fillbytes];
  memset(ffs, 0xff, fillbytes);
  pack_data(packets, ffs, fillbytes);
  

  // Prepare lighting programs

  // First store custom_lights
  custom_light_program(packets, prog.custom_lights);
  
  
  // Now store flashlights
  for (auto flash : prog.flashlights)
    custom_light_program(packets, flash);
  
  return packets; 
}



// We have to assemble a complete program to send
vector<packet> custom_program(char layer, program& prog) {

  vector<packet> program;

  // Get the keyboard's attention.
  program.assign(drv_attn.begin(), drv_attn.end());

  
  // Set up the keymap program
  packet keymap_intro(0x21, layer);
  keymap_intro.bytes[2] = 0x01;
  program.push_back(keymap_intro);
  
  vector<packet> keyprog = custom_keymap_program(layer, prog.kmap);
  program.insert(program.end(), keyprog.begin(), keyprog.end());


  // Set up macro program
  packet macro_intro(0x21, layer);
  macro_intro.bytes[2] = 0x04;
  program.push_back(macro_intro);
  
  vector<packet> macroprog = custom_macro_program(layer);
  program.insert(program.end(), macroprog.begin(), macroprog.end());


  // Set up flashlight program
  packet flash_intro(0x21, layer);
  flash_intro.bytes[2] = 0x05;
  program.push_back(flash_intro);
  
  vector<packet> flashprog = custom_flashlight_program(layer, prog);
  program.insert(program.end(), flashprog.begin(), flashprog.end());


  
  // Set up light program
  packet light_intro(0x21, layer);
  light_intro.bytes[2] = 0x06;
  program.push_back(light_intro);


  // Construct the lighting program
  vector<packet> lightprograms = custom_light_programs(layer, prog);
  program.insert(program.end(), lightprograms.begin(), lightprograms.end());


  packet terminator(0x0b, layer);
  program.push_back(terminator);


  // Compute crc for each packet
  for (auto& pkt: program)
    pkt.compute_crc();

  return program;
}

