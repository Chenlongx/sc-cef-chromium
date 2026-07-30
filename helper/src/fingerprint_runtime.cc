#include "fingerprint_runtime.h"
#include "fingerprint_config.h"

#include "../../patches/M1_navigator_network/src/sc_fp_engine.h"
#include "../../patches/M2_canvas_webgl_audio/src/sc_fp_render.h"
#include "../../patches/M3_fonts_webrtc/src/sc_fp_media.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace sc_cef {
namespace {

std::string Esc(const std::string& s) {
  std::ostringstream out;
  for (char c : s) {
    if (c == '\\' || c == '\'' || c == '"') out << '\\';
    if (c == '\n') {
      out << "\\n";
      continue;
    }
    out << c;
  }
  return out.str();
}

std::string ExtractNestedNumber(const std::string& json, const char* object_key, const char* field,
                                const char* fallback) {
  const auto needle = std::string("\"") + object_key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return fallback;
  auto brace = json.find('{', pos);
  auto end = json.find('}', brace);
  if (brace == std::string::npos || end == std::string::npos) return fallback;
  const auto body = json.substr(brace, end - brace + 1);
  const auto fn = std::string("\"") + field + "\"";
  auto fp = body.find(fn);
  if (fp == std::string::npos) return fallback;
  fp = body.find(':', fp);
  if (fp == std::string::npos) return fallback;
  size_t i = fp + 1;
  while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) ++i;
  size_t j = i;
  while (j < body.size() &&
         (std::isdigit(static_cast<unsigned char>(body[j])) || body[j] == '.' || body[j] == '-'))
    ++j;
  if (j == i) return fallback;
  return body.substr(i, j - i);
}

std::string JsStringArray(const std::vector<std::string>& items) {
  std::ostringstream ss;
  ss << "[";
  for (size_t i = 0; i < items.size(); ++i) {
    if (i) ss << ",";
    ss << "'" << Esc(items[i]) << "'";
  }
  ss << "]";
  return ss.str();
}

}  // namespace

bool FingerprintInjectEnabled() {
  const char* v = getenv("SC_CEF_FP_INJECT");
  if (!v) return true;
  return std::string(v) != "0";
}

bool ActivateFingerprintEngines(const std::string& config_path, std::string* error) {
  auto cfg = LoadFingerprintConfig(config_path);
  if (!cfg.ok) {
    if (error) *error = cfg.error;
    return false;
  }

  sc_fp::NavigatorNetworkConfig nav;
  if (!sc_fp::LoadEngineJsonFile(config_path, &nav) || !nav.loaded) {
    if (error) *error = nav.error.empty() ? "M1 engine load failed" : nav.error;
    return false;
  }
  sc_fp::SetActiveConfig(nav);

  sc_fp::RenderSpoofConfig render;
  if (sc_fp::LoadRenderJsonFile(config_path, &render) && render.loaded) {
    sc_fp::SetRenderConfig(render);
  }

  sc_fp::MediaSpoofConfig media;
  if (sc_fp::LoadMediaJsonFile(config_path, &media) && media.loaded) {
    sc_fp::SetMediaConfig(media);
  }

  return true;
}

std::string BuildNativeFingerprintInjectScript(const std::string& engine_json) {
  if (!FingerprintInjectEnabled()) return {};

  std::string ua, platform, vendor, language, timezone;
  int hw = 8;
  double mem = 8;
  std::string webgl_vendor, webgl_renderer;
  uint32_t canvas_seed = 0, audio_seed = 0;
  std::string webrtc_mode = "proxy_only";
  std::vector<std::string> languages;
  std::vector<std::string> fonts;
  std::string ua_platform = "Windows";
  bool ua_mobile = false;

  sc_fp::OverrideUserAgent(&ua);
  sc_fp::OverridePlatform(&platform);
  sc_fp::OverrideVendor(&vendor);
  sc_fp::OverrideAcceptLanguage(&language);
  sc_fp::OverrideTimezoneId(&timezone);
  sc_fp::OverrideHardwareConcurrency(&hw);
  sc_fp::OverrideDeviceMemory(&mem);
  sc_fp::OverrideWebGlVendor(&webgl_vendor);
  sc_fp::OverrideWebGlRenderer(&webgl_renderer);
  const auto& nav = sc_fp::GetActiveConfig();
  languages = nav.languages;
  if (languages.empty() && !nav.language.empty()) languages.push_back(nav.language);
  if (!nav.ua_platform.empty()) ua_platform = nav.ua_platform;
  else if (platform == "Win32") ua_platform = "Windows";
  ua_mobile = nav.ua_mobile;

  const auto& render = sc_fp::GetRenderConfig();
  canvas_seed = render.canvas_seed;
  audio_seed = render.audio_seed;
  const auto& media = sc_fp::GetMediaConfig();
  if (!media.webrtc_mode.empty()) webrtc_mode = media.webrtc_mode;
  fonts = media.font_profile;

  if (ua.empty()) {
    auto find = [&](const char* key) {
      const std::string needle = std::string("\"") + key + "\"";
      auto pos = engine_json.find(needle);
      if (pos == std::string::npos) return std::string{};
      pos = engine_json.find('"', engine_json.find(':', pos) + 1);
      auto end = engine_json.find('"', pos + 1);
      if (pos == std::string::npos || end == std::string::npos) return std::string{};
      return engine_json.substr(pos + 1, end - pos - 1);
    };
    ua = find("userAgent");
    platform = find("platform");
    vendor = find("vendor");
    language = find("language");
    timezone = find("timezone");
  }

  const std::string sw = ExtractNestedNumber(engine_json, "screen", "width", "1280");
  const std::string sh = ExtractNestedNumber(engine_json, "screen", "height", "800");
  const std::string saw = ExtractNestedNumber(engine_json, "screen", "availWidth", sw.c_str());
  const std::string sah = ExtractNestedNumber(engine_json, "screen", "availHeight", sh.c_str());
  const std::string scd = ExtractNestedNumber(engine_json, "screen", "colorDepth", "24");
  const std::string ow = ExtractNestedNumber(engine_json, "extras", "outerWidth", sw.c_str());
  const std::string oh = ExtractNestedNumber(engine_json, "extras", "outerHeight", sh.c_str());
  const std::string mtp = ExtractNestedNumber(engine_json, "extras", "maxTouchPoints", "0");

  std::ostringstream devices_js;
  devices_js << "[";
  for (size_t i = 0; i < media.media_devices.size(); ++i) {
    if (i) devices_js << ",";
    const auto& d = media.media_devices[i];
    devices_js << "{kind:'" << Esc(d.kind) << "',label:'" << Esc(d.label)
               << "',deviceId:'"
               << Esc(d.device_id.empty() ? ("sc-dev-" + std::to_string(i)) : d.device_id)
               << "',groupId:'sc-group-0'}";
  }
  devices_js << "]";

  std::ostringstream js;
  js << "(function(){\n"
     << "if(window.__scCefFpApplied)return;window.__scCefFpApplied=1;\n"
     << "const UA='" << Esc(ua) << "';\n"
     << "const PL='" << Esc(platform) << "';\n"
     << "const VD='" << Esc(vendor) << "';\n"
     << "const LG='" << Esc(language) << "';\n"
     << "const LGS=" << JsStringArray(languages) << ";\n"
     << "const TZ='" << Esc(timezone) << "';\n"
     << "const HW=" << hw << ";\n"
     << "const DM=" << mem << ";\n"
     << "const WGV='" << Esc(webgl_vendor) << "';\n"
     << "const WGR='" << Esc(webgl_renderer) << "';\n"
     << "const CS=" << canvas_seed << ";\n"
     << "const AS=" << audio_seed << ";\n"
     << "const WRM='" << Esc(webrtc_mode) << "';\n"
     << "const FONTS=" << JsStringArray(fonts) << ";\n"
     << "const DEVS=" << devices_js.str() << ";\n"
     << "const UAP='" << Esc(ua_platform) << "';\n"
     << "const UAM=" << (ua_mobile ? "true" : "false") << ";\n"
     << "const SW=" << sw << ",SH=" << sh << ",SAW=" << saw << ",SAH=" << sah << ",SCD=" << scd << ";\n"
     << "const OW=" << ow << ",OH=" << oh << ",MTP=" << mtp << ";\n"
     << "const def=(o,k,v)=>{try{Object.defineProperty(o,k,{get:()=>v,configurable:true});}catch(e){}};\n"
     << "try{def(Navigator.prototype,'userAgent',UA);}catch(e){}\n"
     << "try{def(Navigator.prototype,'appVersion',UA.replace(/^Mozilla\\//,''));}catch(e){}\n"
     << "try{def(Navigator.prototype,'platform',PL);}catch(e){}\n"
     << "try{def(Navigator.prototype,'vendor',VD);}catch(e){}\n"
     << "try{def(Navigator.prototype,'language',LG|| (LGS[0]||'en-US'));}catch(e){}\n"
     << "try{def(Navigator.prototype,'languages',LGS.length?LGS:['en-US']);}catch(e){}\n"
     << "try{def(Navigator.prototype,'hardwareConcurrency',HW);}catch(e){}\n"
     << "try{def(Navigator.prototype,'deviceMemory',DM);}catch(e){}\n"
     << "try{def(Navigator.prototype,'maxTouchPoints',MTP);}catch(e){}\n"
     << "try{def(Navigator.prototype,'webdriver',undefined);}catch(e){}\n"
     << "try{window.chrome=window.chrome||{runtime:{},loadTimes:function(){},csi:function(){},app:{}};}catch(e){}\n"
     << "try{const fakePlugin={name:'Chrome PDF Plugin',filename:'internal-pdf-viewer',description:'Portable Document Format',length:1};"
        "const plist={0:fakePlugin,1:fakePlugin,2:fakePlugin,3:fakePlugin,4:fakePlugin,length:5,"
        "item:function(i){return this[i]},namedItem:function(){return fakePlugin},refresh:function(){}};"
        "def(Navigator.prototype,'plugins',plist);def(Navigator.prototype,'mimeTypes',plist);}catch(e){}\n"
     << "try{const iq=Permissions.prototype.query;Permissions.prototype.query=function(p){"
        "if(p&&p.name==='notifications')return Promise.resolve({state:Notification.permission||'prompt',onchange:null});"
        "return iq.apply(this,arguments);};}catch(e){}\n"
     << "try{const uad={brands:[{brand:'Not;A=Brand',version:'99'},{brand:'Google Chrome',version:'139'},{brand:'Chromium',version:'139'}],mobile:UAM,platform:UAP,getHighEntropyValues:async(h)=>({brands:uad.brands,mobile:UAM,platform:UAP,platformVersion:'15.0.0',architecture:'x86',bitness:'64',model:'',uaFullVersion:'139.0.7258.139',fullVersionList:[{brand:'Google Chrome',version:'139.0.7258.139'},{brand:'Chromium',version:'139.0.7258.139'},{brand:'Not;A=Brand',version:'10.0.0.0'}]})};def(Navigator.prototype,'userAgentData',uad);}catch(e){}\n"
     << "try{def(screen,'width',SW);def(screen,'height',SH);def(screen,'availWidth',SAW);def(screen,'availHeight',SAH);def(screen,'colorDepth',SCD);def(screen,'pixelDepth',SCD);}catch(e){}\n"
     << "try{def(window,'outerWidth',OW);def(window,'outerHeight',OH);def(window,'innerWidth',SW);def(window,'innerHeight',SH);}catch(e){}\n"
     << "if(TZ){\n"
     << "  try{\n"
     << "    const _DTF=Intl.DateTimeFormat;\n"
     << "    Intl.DateTimeFormat=function(...a){if(!a[1])a[1]={};if(!a[1].timeZone)a[1].timeZone=TZ;return new _DTF(...a);};\n"
     << "    Intl.DateTimeFormat.prototype=_DTF.prototype;\n"
     << "    Intl.DateTimeFormat.supportedLocalesOf=_DTF.supportedLocalesOf.bind(_DTF);\n"
     << "  }catch(e){}\n"
     << "  try{\n"
     << "    Date.prototype.getTimezoneOffset=function(){\n"
     << "      const d=this;\n"
     << "      const a=new Date(d.toLocaleString('en-US',{timeZone:'UTC'}));\n"
     << "      const b=new Date(d.toLocaleString('en-US',{timeZone:TZ}));\n"
     << "      return Math.round((a-b)/60000);\n"
     << "    };\n"
     << "  }catch(e){}\n"
     << "}\n"
     << "const patchGL=(proto)=>{if(!proto)return;const gp=proto.getParameter;proto.getParameter=function(p){if(p===37445&&WGV)return WGV;if(p===37446&&WGR)return WGR;return gp.apply(this,arguments);};};\n"
     << "try{patchGL(WebGLRenderingContext&&WebGLRenderingContext.prototype);}catch(e){}\n"
     << "try{patchGL(WebGL2RenderingContext&&WebGL2RenderingContext.prototype);}catch(e){}\n"
     << "if(CS){const noise=(d)=>{const u=d.data;for(let i=0;i<u.length;i+=68){u[i]^=(CS>>(i%8))&1;}};const gid=HTMLCanvasElement.prototype.toDataURL;HTMLCanvasElement.prototype.toDataURL=function(){const c=this.getContext('2d');if(c){try{const i=c.getImageData(0,0,Math.min(this.width||1,16),Math.min(this.height||1,16));noise(i);c.putImageData(i,0,0);}catch(e){}}return gid.apply(this,arguments);};}\n"
     << "if(AS){try{const ab=AudioBuffer.prototype.getChannelData;AudioBuffer.prototype.getChannelData=function(ch){const d=ab.call(this,ch);for(let i=0;i<Math.min(d.length,256);i++){d[i]+=(((AS^(i*2654435761))>>>0)%7-3)*1e-7;}return d;};}catch(e){}}\n"
     << "if(FONTS.length){try{const check=document.fonts&&document.fonts.check.bind(document.fonts);if(check){document.fonts.check=function(f,t){const fam=(f||'').replace(/^\\d+px\\s+/i,'').replace(/[\"']/g,'');if(FONTS.some(x=>fam.indexOf(x)>=0))return true;return check(f,t);};}}catch(e){}}\n"
     << "if(DEVS.length){try{const md=navigator.mediaDevices||{};md.enumerateDevices=async()=>DEVS.map(d=>({...d,toJSON(){return this}}));def(Navigator.prototype,'mediaDevices',md);}catch(e){}}\n"
     << "if(WRM==='disabled'||WRM==='proxy_only'){try{RTCPeerConnection=undefined;webkitRTCPeerConnection=undefined;}catch(e){}}\n"
     << "if(WRM==='disabled'){try{def(Navigator.prototype,'mediaDevices',undefined);}catch(e){}}\n"
     << "})();\n";
  return js.str();
}

}  // namespace sc_cef
