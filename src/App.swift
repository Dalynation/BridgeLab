import Cocoa
import UniformTypeIdentifiers

final class PluginDropView: NSView {
    var canImport: () -> Bool = { false }
    var onDrop: (([URL]) -> Void)?
    private var highlighted = false { didSet { needsDisplay = true } }
    override init(frame: NSRect) {
        super.init(frame: frame)
        registerForDraggedTypes([.fileURL])
        setAccessibilityElement(true)
        setAccessibilityLabel("Drop Windows plugin DLLs or folders here")
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    private func pluginURLs(_ sender: NSDraggingInfo) -> [URL]? {
        guard canImport(), let urls = sender.draggingPasteboard.readObjects(forClasses:[NSURL.self], options:[.urlReadingFileURLsOnly:true]) as? [URL], !urls.isEmpty else {return nil}
        for url in urls {var directory:ObjCBool=false;guard FileManager.default.fileExists(atPath:url.path,isDirectory:&directory),(directory.boolValue || url.pathExtension.lowercased()=="dll") else{return nil}}
        return urls
    }
    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        highlighted = pluginURLs(sender) != nil
        return highlighted ? .copy : []
    }
    override func draggingUpdated(_ sender: NSDraggingInfo) -> NSDragOperation { draggingEntered(sender) }
    override func draggingExited(_ sender: NSDraggingInfo?) { highlighted = false }
    override func prepareForDragOperation(_ sender: NSDraggingInfo) -> Bool { pluginURLs(sender) != nil }
    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        defer { highlighted = false }
        guard let urls = pluginURLs(sender) else { return false }
        onDrop?(urls)
        return true
    }
    override func concludeDragOperation(_ sender: NSDraggingInfo?) { highlighted = false }
    override func draw(_ dirtyRect: NSRect) {
        let path = NSBezierPath(roundedRect: bounds.insetBy(dx: 2, dy: 2), xRadius: 12, yRadius: 12)
        (highlighted ? NSColor.controlAccentColor.withAlphaComponent(0.16) : NSColor.controlBackgroundColor).setFill()
        path.fill()
        (highlighted ? NSColor.controlAccentColor : NSColor.separatorColor).setStroke()
        path.lineWidth = 2
        path.setLineDash([7, 5], count: 2, phase: 0)
        path.stroke()
        let text = highlighted ? "Release to import these plugins" : "Drop Windows plugin DLLs or folders here"
        let attributes: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 16, weight: .medium), .foregroundColor: NSColor.labelColor]
        let size = (text as NSString).size(withAttributes: attributes)
        (text as NSString).draw(at: NSPoint(x: (bounds.width-size.width)/2, y: (bounds.height-size.height)/2), withAttributes: attributes)
    }
}

final class App: NSObject, NSApplicationDelegate {
    var importScopes:[URL]=[]
    var window: NSWindow!
    var output: NSTextView!
    var selected: URL?
    var testSound: NSSound?
    var pendingImports: [URL] = []
    var pluginMenu: NSPopUpButton!
    var installedPlugins: [[String:Any]] = []
    var importResults:[String] = []
    var importBatch = false
    let manager = BridgeManager()
    var busy = false
    var importButton: NSButton!
    var renderButton: NSButton!
    let root = Bundle.main.resourceURL!
    func applicationDidFinishLaunching(_ notification: Notification) {
        let menu = NSMenu()
        let appItem = NSMenuItem()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit BridgeLab", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu
        menu.addItem(appItem)
        let editItem=NSMenuItem();let edit=NSMenu(title:"Edit");edit.addItem(withTitle:"Paste DLLs from Finder",action:#selector(pastePlugins),keyEquivalent:"v");editItem.submenu=edit;menu.addItem(editItem)
        NSApp.mainMenu = menu
        window = NSWindow(contentRect: NSRect(x:0,y:0,width:780,height:650), styleMask:[.titled,.closable,.miniaturizable,.resizable], backing:.buffered, defer:false)
        window.isReleasedWhenClosed = false
        window.minSize = NSSize(width:780,height:650)
        window.title = "BridgeLab 0.9.1 · Windows Plugin Bridge"
        window.center()
        let view = window.contentView!
        let title = NSTextField(labelWithString:"Bring your Windows instruments to Mac.")
        title.font = .systemFont(ofSize:26,weight:.semibold)
        let subtitle = NSTextField(wrappingLabelWithString:"Import Windows DLLs to create separate instruments in FL Studio. Scan once after importing, then add each plugin by its own name.")
        subtitle.textColor = .secondaryLabelColor
        importButton = NSButton(title:"Import Windows DLL…",target:self,action:#selector(importPlugin))
        renderButton = NSButton(title:"Render test note…",target:self,action:#selector(render))
        renderButton.isEnabled = false
        let doctor = NSButton(title:"Check runtime",target:self,action:#selector(checkRuntime))
        let paste=NSButton(title:"Paste DLLs",target:self,action:#selector(pastePlugins))
        let actions = NSStackView(views:[importButton,paste,renderButton,doctor]); actions.spacing = 12
        let drop = PluginDropView(frame: .zero)
        drop.canImport = { [weak self] in self != nil }
        drop.onDrop = { [weak self] urls in self?.queueImports(urls) }
        pluginMenu = NSPopUpButton(frame:.zero,pullsDown:false);pluginMenu.target=self;pluginMenu.action = #selector(selectInstalled);pluginMenu.addItem(withTitle:"No plugins imported yet");pluginMenu.isEnabled=false
        let scroll = NSScrollView();scroll.hasVerticalScroller = true
        output = NSTextView(frame: NSRect(x:0,y:0,width:700,height:220))
        output.isVerticallyResizable = true
        output.isHorizontallyResizable = false
        output.autoresizingMask = [.width]
        output.textContainer?.widthTracksTextView = true
        output.isEditable = false;output.font = .monospacedSystemFont(ofSize:12,weight:.regular)
        output.textColor = .labelColor;output.backgroundColor = .textBackgroundColor
        output.textContainerInset = NSSize(width:14,height:14);scroll.documentView = output
        let note = NSTextField(labelWithString:"Experimental bridge · Scan in FL Studio after importing. Each plugin appears under its own name.")
        note.font = .systemFont(ofSize:11);note.textColor = .secondaryLabelColor
        let stack = NSStackView(views:[title,subtitle,drop,pluginMenu,actions,scroll,note]);stack.orientation = .vertical;stack.alignment = .leading;stack.spacing = 18;stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo:view.leadingAnchor,constant:28),stack.trailingAnchor.constraint(equalTo:view.trailingAnchor,constant:-28),stack.topAnchor.constraint(equalTo:view.topAnchor,constant:28),stack.bottomAnchor.constraint(equalTo:view.bottomAnchor,constant:-24),drop.widthAnchor.constraint(equalTo:stack.widthAnchor),drop.heightAnchor.constraint(equalToConstant:90),scroll.widthAnchor.constraint(equalTo:stack.widthAnchor),scroll.heightAnchor.constraint(greaterThanOrEqualToConstant:220),subtitle.widthAnchor.constraint(equalTo:stack.widthAnchor)])
        showWindow();checkRuntime()
    }
    func showWindow() {
        guard let window = window else { return }
        if window.isMiniaturized { window.deminiaturize(nil) }
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps:true)
    }
    func applicationShouldHandleReopen(_ sender:NSApplication, hasVisibleWindows:Bool)->Bool {
        showWindow()
        return true
    }
    func application(_ sender: NSApplication, openFiles filenames: [String]) {
        showWindow()
        queueImports(filenames.map{URL(fileURLWithPath:$0)})
        sender.reply(toOpenOrPrint:.success)
    }
    func queueImports(_ urls:[URL]) {for url in urls {if url.startAccessingSecurityScopedResource(){importScopes.append(url)}};if !importBatch {importResults=[];importBatch=true};pendingImports.append(contentsOf:urls);if output != nil && !busy {nextImport()}}
    func nextImport(){guard !busy,!pendingImports.isEmpty else{return};inspectPlugin(pendingImports.removeFirst())}
    @objc func selectInstalled(){let index=pluginMenu.indexOfSelectedItem;guard installedPlugins.indices.contains(index),let path=installedPlugins[index]["path"] as? String else{return};selected=URL(fileURLWithPath:path);renderButton.isEnabled = !busy}
    func refreshPlugins(_ rows:[[String:Any]],preferred:String?){installedPlugins=rows;pluginMenu.removeAllItems();if rows.isEmpty{pluginMenu.addItem(withTitle:"No plugins imported yet");pluginMenu.isEnabled=false;return};for row in rows{pluginMenu.addItem(withTitle:row["name"] as? String ?? "Windows instrument")};pluginMenu.isEnabled=true;if let id=preferred,let index=rows.firstIndex(where:{$0["id"] as? String == id}){pluginMenu.selectItem(at:index)};selectInstalled()}
    @objc func pastePlugins(){
        guard let urls=NSPasteboard.general.readObjects(forClasses:[NSURL.self],options:[.urlReadingFileURLsOnly:true]) as? [URL],!urls.isEmpty else{output.string="Copy plugin DLL files in Finder, then click Paste DLLs.";return}
        queueImports(urls)
    }
    @objc func checkRuntime(){run(["doctor"])}
    @objc func importPlugin(){
        guard !busy else{return}
        let panel = NSOpenPanel();panel.canChooseDirectories = true;panel.canChooseFiles = true;panel.allowsMultipleSelection = true;panel.allowedContentTypes = [.folder,UTType(filenameExtension:"dll") ?? .data];panel.allowsOtherFileTypes = false;panel.treatsFilePackagesAsDirectories = false
        panel.message = "Choose DLLs or their extracted plugin folder. Choosing the folder also permits copying adjacent presets and resources."
        panel.beginSheetModal(for:window){response in if response == .OK{self.queueImports(panel.urls)}}
    }
    func inspectPlugin(_ url: URL) {
        guard !busy else { pendingImports.append(url); return }
        showWindow()
        renderButton.isEnabled = false
        run(["import",url.path],candidate:url)
    }
    @objc func render(){
        guard !busy, let url = selected else{return}
        let panel = NSSavePanel();panel.nameFieldStringValue = "BridgeLab-test.wav";panel.message = "Runs the selected plugin in a separate process and renders middle C. Choose a new filename."
        panel.beginSheetModal(for:window){response in if response == .OK,let destination=panel.url{self.run(["render",url.path,destination.path])}}
    }
    func run(_ args:[String],candidate:URL? = nil){
        guard !busy else{return};busy = true;importButton.isEnabled = false;renderButton.isEnabled = false;output.string = args.first == "import" ? "Importing the plugin. First use prepares its Windows runtime and may take a minute or more…" : "Preparing BridgeLab…"
        DispatchQueue.global().async {
            var result:[String:Any] = [:];var text="";var success=false
            do {result=try self.manager.run(args);let data=try JSONSerialization.data(withJSONObject:result,options:[.prettyPrinted,.sortedKeys]);text=String(data:data,encoding:.utf8) ?? "";success=true}
            catch{text=error.localizedDescription;self.manager.record("FAILED: "+args.joined(separator:" ")+" - "+text)}
            DispatchQueue.main.async {
                self.busy=false;self.importButton.isEnabled=true
                if success,let rows=result["plugins"] as? [[String:Any]]{let plugin=result["plugin"] as? [String:Any];self.refreshPlugins(rows,preferred:plugin?["id"] as? String)}
                self.renderButton.isEnabled=self.selected != nil
                if args.first=="import" {
                    let name=candidate?.lastPathComponent ?? "Plugin"
                    self.importResults.append((success ? "Imported: " : "FAILED: ")+name+(success ? "":"\n"+text))
                    if let warnings=result["warnings"] as? [String]{self.importResults.append(contentsOf:warnings)}
                    self.output.string=self.importResults.joined(separator:"\n\n")+"\n\nScan in FL Studio to find each imported instrument by name."
                    if self.pendingImports.isEmpty {self.importBatch=false;for url in self.importScopes{url.stopAccessingSecurityScopedResource()};self.importScopes=[]}
                }else if let message=result["message"] as? String{self.output.string=message}else{self.output.string=text}
                if success && args.first=="render",let filename=args.last{self.testSound=NSSound(contentsOfFile:filename,byReference:true);self.testSound?.play()}
                self.nextImport()
            }
        }
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender:NSApplication)->Bool {true}
}
@main struct BridgeLabMain {
    static func main(){
        if CommandLine.arguments.count>1 {
            do{let result=try BridgeManager().run(Array(CommandLine.arguments.dropFirst()));let data=try JSONSerialization.data(withJSONObject:result,options:[.prettyPrinted,.sortedKeys]);FileHandle.standardOutput.write(data);FileHandle.standardOutput.write(Data("\n".utf8))}
            catch{FileHandle.standardError.write(Data((error.localizedDescription+"\n").utf8));exit(1)}
            return
        }
        let app=NSApplication.shared;let delegate=App();app.delegate=delegate;app.setActivationPolicy(.regular);app.run()
    }
}
