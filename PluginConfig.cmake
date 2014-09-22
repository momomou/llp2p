#/**********************************************************\ 
#
# Auto-Generated Plugin Configuration file
# for llp2pFB
#
#\**********************************************************/

set(PLUGIN_NAME "llp2pFB")
set(PLUGIN_PREFIX "LFB")
set(COMPANY_NAME "HSNL")

# ActiveX constants:
set(FBTYPELIB_NAME llp2pFBLib)
set(FBTYPELIB_DESC "llp2pFB 1.0 Type Library")
set(IFBControl_DESC "llp2pFB Control Interface")
set(FBControl_DESC "llp2pFB Control Class")
set(IFBComJavascriptObject_DESC "llp2pFB IComJavascriptObject Interface")
set(FBComJavascriptObject_DESC "llp2pFB ComJavascriptObject Class")
set(IFBComEventSource_DESC "llp2pFB IFBComEventSource Interface")
set(AXVERSION_NUM "1")

# NOTE: THESE GUIDS *MUST* BE UNIQUE TO YOUR PLUGIN/ACTIVEX CONTROL!  YES, ALL OF THEM!
set(FBTYPELIB_GUID 01aae0ba-d044-5d0a-8a62-1edc05b1fdc3)
set(IFBControl_GUID ca5667b4-6aae-59c6-b259-f40fd48939a1)
set(FBControl_GUID db33a56e-9a18-53e3-b2fa-2a8b4c66de7a)
set(IFBComJavascriptObject_GUID 3f939866-2aeb-5373-97e8-f15518a3e762)
set(FBComJavascriptObject_GUID 923e28fe-806c-5a1f-a238-419bcfa4cadf)
set(IFBComEventSource_GUID 23a0ac96-5da1-5eca-8012-5de161d3c2ac)
if ( FB_PLATFORM_ARCH_32 )
    set(FBControl_WixUpgradeCode_GUID f6423dae-afd7-5491-b70d-1ec7c729d129)
else ( FB_PLATFORM_ARCH_32 )
    set(FBControl_WixUpgradeCode_GUID 9b1a2fae-209f-56e6-a74e-f0c27d7c74ee)
endif ( FB_PLATFORM_ARCH_32 )

# these are the pieces that are relevant to using it from Javascript
set(ACTIVEX_PROGID "HSNL.llp2pFB")
if ( FB_PLATFORM_ARCH_32 )
    set(MOZILLA_PLUGINID "hsnl.com/llp2pFB")  # No 32bit postfix to maintain backward compatability.
else ( FB_PLATFORM_ARCH_32 )
    set(MOZILLA_PLUGINID "hsnl.com/llp2pFB_${FB_PLATFORM_ARCH_NAME}")
endif ( FB_PLATFORM_ARCH_32 )

# strings
set(FBSTRING_CompanyName "HSNL")
set(FBSTRING_PluginDescription "This is FireBreath version of llp2p")
set(FBSTRING_PLUGIN_VERSION "1.0.0.0")
set(FBSTRING_LegalCopyright "Copyright 2014 HSNL")
set(FBSTRING_PluginFileName "np${PLUGIN_NAME}")
set(FBSTRING_ProductName "llp2pFB")
set(FBSTRING_FileExtents "")
if ( FB_PLATFORM_ARCH_32 )
    set(FBSTRING_PluginName "llp2pFB")  # No 32bit postfix to maintain backward compatability.
else ( FB_PLATFORM_ARCH_32 )
    set(FBSTRING_PluginName "llp2pFB_${FB_PLATFORM_ARCH_NAME}")
endif ( FB_PLATFORM_ARCH_32 )
set(FBSTRING_MIMEType "application/x-llp2pfb")

# Uncomment this next line if you're not planning on your plugin doing
# any drawing:

#set (FB_GUI_DISABLED 1)

# Mac plugin settings. If your plugin does not draw, set these all to 0
set(FBMAC_USE_QUICKDRAW 0)
set(FBMAC_USE_CARBON 1)
set(FBMAC_USE_COCOA 1)
set(FBMAC_USE_COREGRAPHICS 1)
set(FBMAC_USE_COREANIMATION 0)
set(FBMAC_USE_INVALIDATINGCOREANIMATION 0)

# If you want to register per-machine on Windows, uncomment this line
#set (FB_ATLREG_MACHINEWIDE 1)
