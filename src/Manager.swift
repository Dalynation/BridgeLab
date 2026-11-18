import Foundation
import CryptoKit
import Darwin

final class BridgeManager {
    static let version = "0.9.5"
    static let runtimeProfile = "crossover-shared"
    let fm = FileManager.default
    let root: URL
    let resources: URL
    init() {
        root = URL(fileURLWithPath: ProcessInfo.processInfo.environment["BRIDGELAB_HOME"] ?? (NSHomeDirectory()+"/Library/Application Support/BridgeLab"))
        resources = Bundle.main.resourceURL!
    }
    func failure(_ text:String)->NSError {NSError(domain:"BridgeLab",code:1,userInfo:[NSLocalizedDescriptionKey:text])}
    func hash(_ data:Data)->String {SHA256.hash(data:data).map{String(format:"%02x",$0)}.joined()}
    func writeJSON(_ value:Any,_ url:URL)throws {try JSONSerialization.data(withJSONObject:value,options:[.prettyPrinted,.sortedKeys]).write(to:url,options:.atomic)}
    func record(_ text:String){
        let url=root.appendingPathComponent("logs/imports.log")
        let line=Data((ISO8601DateFormatter().string(from:Date())+" "+text+"\n").utf8)
        if !fm.fileExists(atPath:url.path){fm.createFile(atPath:url.path,contents:nil)}
        if let file=try? FileHandle(forWritingTo:url){file.seekToEndOfFile();file.write(line);try? file.close()}
    }
    func plugins()->[[String:Any]] {guard let data=try? Data(contentsOf:root.appendingPathComponent("plugins.json")),let rows=(try? JSONSerialization.jsonObject(with:data)) as? [[String:Any]] else{return []};return rows}
    func execute(_ url:URL,_ arguments:[String],env:[String:String]? = nil,timeout:Double=120)throws->String {
        let p=Process();p.executableURL=url;p.arguments=arguments;p.environment=env
        // Wine services may outlive their launcher and inherit stdout. A pipe
        // would wait for those services forever even after the launcher exits.
        let output=fm.temporaryDirectory.appendingPathComponent("bridgelab-command-"+UUID().uuidString)
        fm.createFile(atPath:output.path,contents:nil)
        let handle=try FileHandle(forWritingTo:output)
        defer{try? handle.close();try? fm.removeItem(at:output)}
        p.standardOutput=handle;p.standardError=handle
        try p.run()
        let watchdog=DispatchWorkItem{if p.isRunning {p.terminate();DispatchQueue.global().asyncAfter(deadline:.now()+3){if p.isRunning{kill(p.processIdentifier,SIGKILL)}}}}
        DispatchQueue.global().asyncAfter(deadline:.now()+timeout,execute:watchdog)
        p.waitUntilExit();watchdog.cancel()
        let data=(try? Data(contentsOf:output)) ?? Data()
        let text=String(data:data,encoding:.utf8) ?? "Unreadable runtime response"
        guard p.terminationStatus==0 else{throw failure("Command failed (\(p.terminationStatus)): \(text.suffix(3000))")}
        return text.trimmingCharacters(in:.whitespacesAndNewlines)
    }
    func installEngine()throws {
        for dir in ["logs","plugins","prefixes","backups"] {try fm.createDirectory(at:root.appendingPathComponent(dir),withIntermediateDirectories:true)}
        let current=root.appendingPathComponent("engine"),payload=resources.appendingPathComponent("Engine")
        if (try? String(contentsOf:current.appendingPathComponent("version.txt"),encoding:.utf8).trimmingCharacters(in:.whitespacesAndNewlines)) == Self.version{return}
        guard fm.fileExists(atPath:payload.appendingPathComponent("version.txt").path) else{throw failure("This app is missing its engine payload. Reinstall the complete BridgeLab app.")}
        let stage=root.appendingPathComponent("engine-staging-"+UUID().uuidString)
        _=try execute(URL(fileURLWithPath:"/usr/bin/ditto"),[payload.path,stage.path],timeout:300)
        if fm.fileExists(atPath:current.path){try fm.moveItem(at:current,to:root.appendingPathComponent("backups/engine-"+UUID().uuidString))}
        try fm.moveItem(at:stage,to:current)
    }
    func inspect(_ url:URL)throws->[String:Any] {
        guard url.pathExtension.lowercased()=="dll" else{throw failure("Unzip the plugin and choose its Windows .dll file.")}
        let data=try Data(contentsOf:url,options:.mappedIfSafe)
        func u16(_ o:Int)->UInt16 {UInt16(data[o]) | UInt16(data[o+1])<<8}
        func u32(_ o:Int)->UInt32 {UInt32(u16(o)) | UInt32(u16(o+2))<<16}
        guard data.count>=64,data[0]==77,data[1]==90 else{throw failure("This is not a Windows DLL.")}
        let off=Int(u32(60));guard off>=64,off<=data.count-26,Array(data[off..<off+4])==[80,69,0,0] else{throw failure("Invalid PE header.")}
        let cpu=u16(off+4),sections=Int(u16(off+6)),optional=Int(u16(off+20))
        guard sections>0,optional>=2,off+24+optional+sections*40<=data.count,u16(off+22)&0x2000 != 0 else{throw failure("Truncated or unsupported DLL.")}
        guard (cpu==0x14c && u16(off+24)==0x10b)||(cpu==0x8664 && u16(off+24)==0x20b) else{throw failure("Supported Windows DLL architectures: x86 and x64. Windows ARM and Windows VST3 are not supported.")}
        return ["path":url.path,"architecture":cpu==0x14c ? "x86":"x64","sha256":hash(data)]
    }
    func profile(_ arch:String)throws->(URL,[String:String]) {
        var env=ProcessInfo.processInfo.environment
        for key in ["__CFBundleIdentifier","XPC_SERVICE_NAME","WINEPREFIX","WINEDEBUG"] {env.removeValue(forKey:key)}
        env["WINEPREFIX"]=root.appendingPathComponent("prefixes/"+Self.runtimeProfile).path;env["WINEARCH"]="win64";env["WINEDEBUG"]="-all";env["MVK_CONFIG_LOG_LEVEL"]="0";env["WINE_D3D_CONFIG"]="renderer=gl,shader_backend=glsl"
        let runtime="Wine Crossover.app/Contents/Resources/wine/bin/wine64"
        let executable=root.appendingPathComponent("engine/runtime/"+runtime)
        let prefix=root.appendingPathComponent("prefixes/"+Self.runtimeProfile)
        let ready=prefix.appendingPathComponent(".bridgelab-ready")
        if !fm.fileExists(atPath:ready.path){
            // A registry file can appear before Wine finishes copying its system DLLs.
            // Preserve interrupted setup and initialize a complete, separate profile.
            if fm.fileExists(atPath:prefix.path){
                let server=executable.deletingLastPathComponent().appendingPathComponent("wineserver")
                _=try? execute(server,["-k"],env:env,timeout:10)
                try fm.moveItem(at:prefix,to:root.appendingPathComponent("backups/interrupted-"+arch+"-"+UUID().uuidString))
            }
            record("Preparing fresh runtime: "+arch)
            _=try execute(executable,["wineboot.exe","--init"],env:env,timeout:180)
            let required=["system.reg","drive_c/windows/system32/kernel32.dll","drive_c/windows/syswow64/kernel32.dll"]
            let deadline=Date().addingTimeInterval(30)
            while !required.allSatisfy({fm.fileExists(atPath:prefix.appendingPathComponent($0).path)}) && Date()<deadline {Thread.sleep(forTimeInterval:0.1)}
            guard required.allSatisfy({fm.fileExists(atPath:prefix.appendingPathComponent($0).path)}) else{throw failure("Windows runtime setup did not finish. Import again to retry; the incomplete setup will be preserved.")}
            try "ready\n".write(to:ready,atomically:true,encoding:.utf8)
        }
        return (executable,env)
    }
    func windows(_ url:URL)->String {"Z:"+url.path}
    var pluginDirectory:URL {URL(fileURLWithPath:ProcessInfo.processInfo.environment["BRIDGELAB_PLUGIN_DIR"] ?? (NSHomeDirectory()+"/Library/Audio/Plug-Ins/VST3"))}
    func pluginFolderFailure(_ folder:URL,_ error:Error)->NSError {
        let attributes=(try? fm.attributesOfItem(atPath:folder.path)) ?? [:]
        let owner=attributes[.ownerAccountName] as? String ?? "unknown"
        let mode=(attributes[.posixPermissions] as? NSNumber).map{String(format:"%03o",$0.intValue)} ?? "unknown"
        return failure("BridgeLab cannot install an instrument in:\n\(folder.path)\n\nIn Finder, press Command-Shift-G and enter that path. Select the VST3 folder, choose File > Get Info, and check Sharing & Permissions. Your account (\(NSUserName())) needs Read & Write access. Also check that the folder is not Locked. If another account owns this folder, ask the Mac administrator to repair this folder's access. BridgeLab will not change permissions on your other plugins.\n\nAfter access is repaired, import the DLL again. No FL Studio rescan is needed until import succeeds.\n\nDetails: owner=\(owner), mode=\(mode). \(error.localizedDescription)")
    }
    func preparePluginDirectory()throws->URL {
        let folder=pluginDirectory
        let probe=folder.appendingPathComponent(".BridgeLab-write-check-"+UUID().uuidString)
        defer {try? fm.removeItem(at:probe)}
        do {
            try fm.createDirectory(at:folder,withIntermediateDirectories:true)
            // Exercise actual creation, writing and renaming: a writable flag alone
            // misses inherited ACL restrictions and read-only filesystems.
            try fm.createDirectory(at:probe,withIntermediateDirectories:false)
            let first=probe.appendingPathComponent("write"),second=probe.appendingPathComponent("rename")
            try Data("BridgeLab permission check".utf8).write(to:first)
            try fm.moveItem(at:first,to:second)
            try fm.removeItem(at:second)
            try fm.removeItem(at:probe)
        } catch {throw pluginFolderFailure(folder,error)}
        return folder
    }
    func importPlugin(_ source:URL)throws->[String:Any] {
        let info=try inspect(source)
        let pluginsFolder=try preparePluginDirectory()
        try installEngine();record("Importing "+source.path)
        let digest=info["sha256"] as! String,id=String(digest.prefix(16)),arch=info["architecture"] as! String
        var rows=plugins()
        if let existing=rows.first(where:{$0["id"] as? String == id}),let bundle=existing["wrapper"] as? String,fm.fileExists(atPath:bundle),existing["bridge_version"] as? String == Self.version,(existing["warnings"] as? [String] ?? []).isEmpty{return ["plugin":existing,"plugins":rows,"message":"Already imported. Each FL Studio instance retains its own plugin and state."]}
        let folder=root.appendingPathComponent("plugins/"+id);try fm.createDirectory(at:folder,withIntermediateDirectories:true)
        let dll=folder.appendingPathComponent(source.lastPathComponent)
        if !fm.fileExists(atPath:dll.path){try Data(contentsOf:source).write(to:dll,options:.atomic)}
        var warnings:[String]=[]
        // Common sidecars are copied; external libraries and vendor installers still need their own setup.
        for candidate in [source.deletingPathExtension(),source.deletingPathExtension().appendingPathExtension("ini")] {
            let destination=folder.appendingPathComponent(candidate.lastPathComponent)
            if fm.fileExists(atPath:candidate.path){do{_=try execute(URL(fileURLWithPath:"/usr/bin/ditto"),["--norsrc","--noextattr","--noacl",candidate.path,destination.path],timeout:15)}
                catch{let warning="Could not copy optional resources at "+candidate.path+": "+error.localizedDescription;warnings.append(warning);record(warning)}}
        }
        let (wine,env)=try profile(arch)
        let probe=try execute(wine,[root.appendingPathComponent("engine/bin/runner-"+arch+".exe").path,windows(dll),"--probe"],env:env,timeout:180)
        let stem=source.deletingPathExtension().lastPathComponent
        let display="BridgeLab - "+String(stem.prefix(40))+" ("+arch+")"
        let uid=String(hash(Data(("BridgeLab.processor."+digest).utf8)).prefix(32)).uppercased()
        let controller=String(hash(Data(("BridgeLab.controller."+digest).utf8)).prefix(32)).uppercased()
        let safe=stem.map{("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_".contains($0)) ? String($0):"_"}.joined()
        let target=pluginsFolder.appendingPathComponent("BridgeLab-"+safe+"-"+id+".vst3")
        let stage=pluginsFolder.appendingPathComponent(".BridgeLab-staging-"+UUID().uuidString+".vst3")
        defer {try? fm.removeItem(at:stage)}
        do {
            _=try execute(URL(fileURLWithPath:"/usr/bin/ditto"),["--norsrc","--noextattr","--noacl",root.appendingPathComponent("engine/BridgeLabInstrument.vst3").path,stage.path])
        } catch {
            // Access can change after preflight. Recheck before reporting a raw
            // copy error; preserve the original diagnostic if access still works.
            _=try preparePluginDirectory()
            throw failure("Could not create the BridgeLab instrument. Import again; if it still fails, send this message to support.\n"+error.localizedDescription)
        }
        let res=stage.appendingPathComponent("Contents/Resources")
        try (uid+"\n"+controller+"\n"+display+"\n"+dll.path+"\n"+Self.runtimeProfile+"\n").write(to:res.appendingPathComponent("bridgelab.conf"),atomically:true,encoding:.utf8)
        let plistURL=stage.appendingPathComponent("Contents/Info.plist")
        var plist=(try PropertyListSerialization.propertyList(from:Data(contentsOf:plistURL),format:nil)) as! [String:Any]
        plist["CFBundleIdentifier"]="local.bridgelab.plugin."+id;plist["CFBundleName"]=display
        try PropertyListSerialization.data(fromPropertyList:plist,format:.xml,options:0).write(to:plistURL,options:.atomic)
        let metadata=res.appendingPathComponent("moduleinfo.json")
        var module=(try JSONSerialization.jsonObject(with:Data(contentsOf:metadata))) as! [String:Any]
        module["Name"]=display
        if var classes=module["Classes"] as? [[String:Any]] {for i in classes.indices {let processor=classes[i]["Category"] as? String == "Audio Module Class";classes[i]["CID"]=processor ? uid:controller;classes[i]["Name"]=processor ? display:"BridgeLab Controller"};module["Classes"]=classes}
        try writeJSON(module,metadata)
        _=try execute(URL(fileURLWithPath:"/usr/bin/codesign"),["--force","--sign","-",metadata.path])
        _=try execute(URL(fileURLWithPath:"/usr/bin/codesign"),["--force","--sign","-",stage.path])
        _=try execute(URL(fileURLWithPath:"/usr/bin/codesign"),["--verify","--deep","--strict",stage.path])
        if fm.fileExists(atPath:target.path){try fm.moveItem(at:target,to:root.appendingPathComponent("backups/"+UUID().uuidString+".vst3"))}
        try fm.moveItem(at:stage,to:target)
        let row:[String:Any]=["id":id,"name":display,"path":dll.path,"original":source.path,"architecture":arch,"sha256":digest,"processor_uid":uid,"controller_uid":controller,"wrapper":target.path,"probe":probe,"bridge_version":Self.version,"runtime_profile":Self.runtimeProfile,"warnings":warnings]
        rows.removeAll{$0["id"] as? String == id};rows.append(row);rows.sort{($0["name"] as? String ?? "") < ($1["name"] as? String ?? "")}
        try writeJSON(rows,root.appendingPathComponent("plugins.json"))
        record("Installed "+display)
        return ["plugin":row,"plugins":rows,"warnings":warnings,"message":"Installed \(display). In FL Studio: Manage plugins → Find installed plugins, then Add this instrument. Existing imports remain available."]
    }
    func importSelection(_ source:URL)throws->[String:Any] {
        var directory:ObjCBool=false
        if fm.fileExists(atPath:source.path,isDirectory:&directory),directory.boolValue {
            let dlls=try fm.contentsOfDirectory(atPath:source.path).filter{($0 as NSString).pathExtension.lowercased()=="dll"}.sorted()
            guard !dlls.isEmpty else{throw failure("This folder has no DLLs. Choose the extracted folder containing the Windows plugin DLL.")}
            var result:[String:Any]=[:],warnings:[String]=[]
            for name in dlls {result=try importPlugin(source.appendingPathComponent(name));warnings += result["warnings"] as? [String] ?? []}
            result["warnings"]=warnings;return result
        }
        return try importPlugin(source)
    }
    func render(_ url:URL,to output:URL)throws->[String:Any] {
        let info=try inspect(url);try installEngine();guard !fm.fileExists(atPath:output.path),output.pathExtension.lowercased()=="wav" else{throw failure("Choose a new .wav filename.")}
        let arch=info["architecture"] as! String,(wine,env)=try profile(arch)
        let result=try execute(wine,[root.appendingPathComponent("engine/bin/runner-"+arch+".exe").path,windows(url),windows(output)],env:env)
        return ["output":output.path,"result":result]
    }
    func editorHealth(in directory:URL? = nil)->[[String:Any]] {
        let directory=directory ?? fm.temporaryDirectory
        guard let files=try? fm.contentsOfDirectory(at:directory,includingPropertiesForKeys:[.fileSizeKey]) else{return []}
        return files.filter{$0.lastPathComponent.hasPrefix("BridgeLab-editor-") && $0.lastPathComponent.hasSuffix(".health.json")}.compactMap { url in
            let mapping=URL(fileURLWithPath:String(url.path.dropLast(".health.json".count)))
            guard let size=(try? fm.attributesOfItem(atPath:url.path)[.size]) as? NSNumber,size.intValue<=4096,
                  let mappedSize=(try? fm.attributesOfItem(atPath:mapping.path)[.size]) as? NSNumber,mappedSize.intValue==16+2048*2048*4,
                  let data=try? Data(contentsOf:url),let value=(try? JSONSerialization.jsonObject(with:data)) as? [String:Any],value["format_version"] as? Int==1 else{return nil}
            var observation=value
            let attributes=try? fm.attributesOfItem(atPath:url.path)
            if let date=attributes?[.modificationDate] as? Date {observation["status_stale"]=Date().timeIntervalSince(date)>5}
            return observation
        }
    }
    func editorHealthMessage(_ observations:[[String:Any]])->String {
        if observations.isEmpty{return "Editor controls are not certified by import or audio rendering. No popup observations are available from open editors yet."}
        let modes=observations.reduce(0){$0 | ($1["unsupported_popup_modes"] as? Int ?? 0)}
        var messages:[String]=[]
        if modes&1 != 0 {messages.append("An open plugin uses GPU popup rendering that BridgeLab cannot yet forward.")}
        if modes&2 != 0 {messages.append("An open plugin exceeds the supported popup size or window limits; some popup content is not forwarded.")}
        if observations.contains(where:{$0["capture_failure"] as? Int == 1}){messages.append("An open plugin's popup could not be copied. Some menus may be missing.")}
        if observations.contains(where:{($0["input_rejected"] as? Int ?? 0) > 0}) {messages.append("Some input could not reach an open plugin editor.")}
        if observations.contains(where:{$0["frame_stalled"] as? Int == 1 || $0["status_stale"] as? Bool == true}) {messages.append("An open editor has stopped updating its display or status. Reopen it before continuing.")}
        for observation in observations where observation["audio_blocks_observed"] != nil || observation["quiet_while_notes"] != nil || observation["nonfinite_audio_samples"] != nil || observation["above_full_scale_recent"] != nil || observation["audio_observation_stale"] != nil {
            let name=(observation["instrument_name"] as? String).flatMap{$0.isEmpty ? nil:$0} ?? "Open instrument"
            let stale=observation["audio_observation_stale"] as? Int == 1
            if stale {messages.append(name+": no recent audio blocks. Check host playback and channel mute/routing before treating it as a plugin failure.")}
            else if observation["quiet_while_notes"] as? Int == 1 {messages.append(name+": output is very quiet while notes are held. This can be intentional. Check volume, filter cutoff/type, envelope and oscillator mix, then channel routing. BridgeLab preserves these settings.")}
            else if let level=observation["audio_rms"] as? Double,level>0.00177827941 {messages.append(name+": audio is being produced. If you cannot hear it, check the DAW channel/mixer routing and output device.")}
            else if observation["note_ons_observed"] as? Int == 0 {messages.append(name+": no note-on events received yet. Select this channel and play a note; check MIDI input and mute/routing if it stays silent.")}
            if (observation["nonfinite_audio_samples"] as? Int ?? 0)>0 {messages.append(name+": the plugin produced invalid audio samples; those samples were silenced. This is separate from a quiet patch.")}
            if !stale && (observation["above_full_scale_recent"] as? Int ?? 0)>0 {messages.append(name+": some output exceeded digital full scale. Check gain if you hear unwanted distortion; the host may provide additional headroom.")}
        }
        if messages.isEmpty {
            if observations.contains(where:{($0["popup_windows_detected"] as? Int ?? 0)>0}) {messages.append("Original popup forwarding has been observed. Menu selections and individual controls still require testing.")}
            else {messages.append("Editor input delivery and display frames are being monitored. Individual controls have not been automatically certified.")}
        }
        return messages.joined(separator:"\n")
    }
    func run(_ args:[String])throws->[String:Any] {
        switch args.first {
        case "import":guard args.count==2 else{throw failure("Choose a DLL.")};return try importSelection(URL(fileURLWithPath:args[1]))
        case "inspect":return try inspect(URL(fileURLWithPath:args[1]))
        case "render":return try render(URL(fileURLWithPath:args[1]),to:URL(fileURLWithPath:args[2]))
        default:try installEngine();let health=editorHealth();return ["editor_health":health,"version":Self.version,"installation":root.path,"plugins":plugins(),"message":"Import Windows VST2 instrument DLLs. Each import gets its own FL Studio entry. Apple Silicon requires Rosetta for the Windows runtime. Minimum target: macOS 11; compatibility is verified per plugin and host.\n\n"+editorHealthMessage(health)]
        }
    }
}
