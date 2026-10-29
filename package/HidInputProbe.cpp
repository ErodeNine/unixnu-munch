// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// Starts the ShengHidInput device on this board.
//
// NativeHidInput is an IOHIDDevice compiled into this kernel, not a kext, so IOKit has no
// personality to match it against and nothing would ever start it. The driver adaption guide
// names the two ways a service starts: one matched through its provider, which needs a
// personality and probe/start, and one the board starts explicitly once its resources and OS
// dependencies are ready. A statically linked driver is the second kind, and the same guide
// points at the board initialisation calls in native_legacy_display_late_probe as where those
// calls belong, which is where the touch core is started from.
//
// The sequence itself is the reference implementation's, not something the documentation
// describes: register the class (which is why its static initialiser was globalised when the
// object was built), then allocate, init, attach and start. UniXNU-sheng and UniXNU-pipa both
// do exactly this; the only board-specific parts here are the boot argument and the provider.
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSIterator.h>
#include <libkern/c++/OSKext.h>
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSSymbol.h>
#include <pexpert/pexpert.h>
#include <mach/kmod.h>

extern kmod_info_t g_kernel_kmod_info;
// The static initialiser of the publisher, globalised by build-sheng-hid-input.py so that a
// statically linked class can be registered from here instead of by a kext load.
extern "C" void native_hid_static_init(void) __asm__("__GLOBAL__sub_I_NativeHidInput.cpp");
extern "C" IOService *native_hid_input_allocate(void);
extern "C" IOService *native_hid_touch_allocate(void);
extern "C" kern_return_t OSKextLoadKextWithIdentifier(const char *);

extern "C" {
// Published for the diagnostic panel and the console report.
volatile int native_hid_matching_load_status = -1;
volatile uint32_t native_hid_stage, native_hid_resource_found;
volatile int native_hid_load_status = -1, native_hid_input_meta_status = -1;
volatile uint32_t native_hid_class_sizes[5];
volatile uint64_t native_hid_input_object, native_hid_touch_object;
// 0 not asked for, 1 started, 2 the device refused to start.
volatile uint32_t native_hid_touch_status;
// The publisher's tick asks whether it should run. The touch core feeds it directly through
// the sink, so nothing has to poll: answer yes once the board has asked for the digitizer.
volatile uint32_t native_hid_input_command;
// 0 nothing published, 1 the event service carries the digitizer keys, 2 its provider does too.
volatile uint32_t native_hid_props_status;
}

bool native_hid_input_requested(void) {
  return true;
}

// The IOHIDEventDriver that attached below this device, if any. It registers asynchronously
// after the device starts, so it is looked up on every tick instead of once.
static IORegistryEntry *hidEventDriver(IORegistryEntry *parent, unsigned depth) {
  if (!depth) return nullptr;
  OSIterator *it = parent->getChildIterator(gIOServicePlane);
  if (!it) return nullptr;
  IORegistryEntry *found = nullptr;
  while (OSObject *child = it->getNextObject()) {
    IORegistryEntry *entry = OSDynamicCast(IORegistryEntry, child);
    if (!entry) continue;
    if (entry->metaCast("IOHIDEventDriver")) { found = entry; break; }
    if ((found = hidEventDriver(entry, depth - 1))) break;
  }
  it->release();
  return found;
}

// ShengHidInput's timer calls this once a second with each device. backboardd classifies a
// digitizer by the properties of the IOHIDEventDriver service it opens, not by the IOHIDDevice
// below it, and it drops a digitizer event whose sending service does not carry
// PrimaryUsagePage 0x0D with PrimaryUsage 1, 4 or 0x20 or the Built-In flag. The descriptor's
// own classification is not enough: setting these on the device would change driver matching,
// which depends on the descriptor layout, so they are published on the matched services
// instead, exactly as the reference board does. OfflineDigitizerProperties marks our own
// publication so this runs once per service rather than on every tick: a repeated setProperty
// keeps backboardd re-evaluating the service.
extern "C" void native_hid_live_checkpoint(IOService *device) {
  if (!device || !device->getProperty("Built-In")) return;
  IORegistryEntry *driver = hidEventDriver(device, 4);
  if (!driver || driver->getProperty("OfflineDigitizerProperties")) return;
  driver->setProperty("OfflineDigitizerProperties", kOSBooleanTrue);
  driver->setProperty("Built-In", kOSBooleanTrue);
  driver->setProperty("DisplayIntegrated", kOSBooleanTrue);
  driver->setProperty("PrimaryUsagePage", (unsigned long long)0x0d, 32);
  driver->setProperty("PrimaryUsage", (unsigned long long)4, 32);
  native_hid_props_status = 1;
  // backboardd's digitizer filter is {DeviceUsagePage, DeviceUsage, Built-In}, and the event
  // service's provider is the interface those are read from on some paths. Both levels get
  // the keys, as on the reference board.
  if (IORegistryEntry *interface = driver->getParentEntry(gIOServicePlane)) {
    interface->setProperty("Built-In", kOSBooleanTrue);
    interface->setProperty("DisplayIntegrated", kOSBooleanTrue);
    interface->setProperty("PrimaryUsagePage", (unsigned long long)0x0d, 32);
    interface->setProperty("PrimaryUsage", (unsigned long long)4, 32);
    native_hid_props_status = 2;
  }
}

// Runs once, from native_legacy_display_late_probe, after the display and the touch core are up.
// Returns 0 when the device started. Idempotent: a second call returns the first result.
extern "C" int native_hid_input_probe(void) {
  static int result = -1;
  if (result >= 0) return result;
  unsigned enabled = 0;
  if (!PE_parse_boot_argn("offline-hid-input", &enabled, sizeof(enabled)) || enabled != 1) {
    result = 1;                       // not asked for on this boot
    return result;
  }
  native_hid_stage = 1;
  // IOHIDFamily must be loaded first: the device attaches to the IOHIDResource it publishes.
  native_hid_load_status = OSKextLoadKextWithIdentifier("com.apple.iokit.IOHIDFamily");
  /* Two kexts, not one. IOHIDFamily provides the device class and IOHIDResource, but the thing
   * that makes a report reach the system is IOHIDEventDriver, which is a separate kext. Without
   * it the publisher starts, publishes, and every report is dropped by its first line
   * (hasEventDriverCached() finds no child service), so native_touch_hid_reports stays zero
   * while contacts arrive. Measured on boot-usb92: stage reached 8 with zero reports. The
   * reference board loads both, in this order. */
  if (!native_hid_load_status)
    native_hid_matching_load_status = OSKext::loadKextWithIdentifier(
        "com.apple.iokit.IOHIDEventDriver", true, false, kOSKextExcludeNone, kOSKextExcludeNone, nullptr);
  const char *names[5] = {"IOHIDUserDevice", "IOHIDEventDriver", "IOHIDEventService",
                          "IOHIDResourceDeviceUserClient", "IOHIDDevice"};
  for (unsigned i = 0; i < 5; ++i) {
    const OSSymbol *name = OSSymbol::withCString(names[i]);
    const OSMetaClass *meta = name ? OSMetaClass::getMetaClassWithName(name) : nullptr;
    if (meta) native_hid_class_sizes[i] = meta->getClassSize();
    OSSafeReleaseNULL(name);
  }
  native_hid_stage = 2;
  OSDictionary *match = IOService::serviceMatching("IOHIDResource");
  IOService *resource = match ? IOService::waitForMatchingService(match, 5000000000ULL) : nullptr;
  OSSafeReleaseNULL(match);
  native_hid_resource_found = resource != nullptr;
  if (resource == nullptr) { native_hid_stage = 3; result = 2; return result; }
  // 208 is the IOHIDDevice instance size this build's headers describe. If it does not match,
  // the class layout and the original vtable disagree and starting it would corrupt memory.
  if (native_hid_class_sizes[4] != 208) { native_hid_stage = 4; result = 3; OSSafeReleaseNULL(resource); return result; }
  void *handle = OSMetaClass::preModLoad(g_kernel_kmod_info.name);
  if (handle == nullptr) { native_hid_stage = 5; result = 4; OSSafeReleaseNULL(resource); return result; }
  native_hid_static_init();
  native_hid_input_meta_status = OSMetaClass::postModLoad(handle);
  if (native_hid_input_meta_status != 0) { native_hid_stage = 6; result = 5; OSSafeReleaseNULL(resource); return result; }
  IOService *device = native_hid_input_allocate();
  native_hid_input_object = (uintptr_t) device;
  bool valid = device && device->metaCast("IOHIDDevice") && device->init();
  bool attached = valid && device->attach(resource);
  bool started = attached && device->start(resource);
  if (!started) {
    native_hid_stage = 7;
    if (attached) device->detach(resource);
    OSSafeReleaseNULL(device);
    native_hid_input_object = 0;
    result = 6;
    OSSafeReleaseNULL(resource);
    return result;
  }
  native_hid_stage = 8;
  // A second IOHIDDevice carrying only the digitizer, and the only one that should carry it.
  // NativeHidInput.cpp:132 states the reason the combined device cannot serve the panel: a
  // digitizer is matched by primary usage page/usage (Digitizer/Touch Screen) plus Built-In,
  // which a keyboard-first descriptor never provides. The split device exists precisely to
  // publish that personality on its own.
  //
  // This used to be gated on sheng-touch-hid=1 as well, so the split device only appeared
  // next to a combined device that was already advertising a Touch Screen collection. Two
  // digitizers on one panel is not a fallback: backboardd keys its touch pipeline off the
  // matched service, and boot-usb64 through boot-usb71 (build-6, 2026-10-29) measured exactly
  // that shape: tsplit=1 with the split device started, hres=0 so the kernel accepted every
  // report, and edge=1,0 because no digitizer event ever reached a dispatch wrapper.
  //
  // The reference board runs one digitizer. sheng-touch-split=1 alone selects the split
  // device; sheng-touch-hid=0 keeps the combined device off the digitizer collection while it
  // still publishes the keyboard, so exactly one Touch Screen service exists on this panel.
  unsigned split = 0;
  if (PE_parse_boot_argn("sheng-touch-split", &split, sizeof split) && split == 1) {
    IOService *touch = native_hid_touch_allocate();
    native_hid_touch_object = (uintptr_t) touch;
    bool touchStarted = touch && touch->metaCast("IOHIDDevice") && touch->init() &&
                        touch->attach(resource) && touch->start(resource);
    if (!touchStarted && touch) {
      touch->detach(resource);
      OSSafeReleaseNULL(touch);
      native_hid_touch_object = 0;
    }
    native_hid_touch_status = touchStarted ? 1 : 2;
  }
  result = 0;
  OSSafeReleaseNULL(resource);
  return result;
}
