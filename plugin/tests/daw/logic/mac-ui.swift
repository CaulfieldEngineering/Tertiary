// [Codex 2026-10-30] Small native Accessibility CLI for repeatable Mac host tests.
import AppKit
import ApplicationServices

func attr(_ e: AXUIElement, _ key: String) -> CFTypeRef? {
    var value: CFTypeRef?
    AXUIElementCopyAttributeValue(e, key as CFString, &value)
    return value
}
func children(_ e: AXUIElement) -> [AXUIElement] { attr(e, "AXChildren") as? [AXUIElement] ?? [] }
func osa(_ source: String) throws {
    let p=Process(); p.executableURL=URL(fileURLWithPath:"/usr/bin/osascript"); p.arguments=["-e",source]
    try p.run(); p.waitUntilExit()
    if p.terminationStatus != 0 { throw NSError(domain:"System Events failed",code:Int(p.terminationStatus)) }
}
func app(_ name: String) throws -> AXUIElement {
    guard let a = NSWorkspace.shared.runningApplications.first(where: { $0.localizedName == name }) else { throw NSError(domain:"App not running: \(name)", code:1) }
    return AXUIElementCreateApplication(a.processIdentifier)
}
func element(_ name: String, _ path: String) throws -> AXUIElement {
    var e = try app(name)
    for part in path.split(separator:".") {
        let c = children(e)
        guard let n = Int(part), c.indices.contains(n) else { throw NSError(domain:"Stale AX path \(path)",code:2) }
        e = c[n]
    }
    return e
}
func snapshot(_ e: AXUIElement, _ path: String, _ depth: Int, _ rows: inout [[String:Any]]) {
    var r:[String:Any] = ["path":path]
    for k in ["AXRole","AXTitle","AXDescription","AXValue","AXIdentifier","AXHelp","AXEnabled"] {
        if let v=attr(e,k) { r[String(k.dropFirst(2))] = (v as? String) ?? (v as? NSNumber)?.stringValue ?? "" }
    }
    if let p=attr(e,"AXPosition"), CFGetTypeID(p)==AXValueGetTypeID(), let s=attr(e,"AXSize"), CFGetTypeID(s)==AXValueGetTypeID() {
        var point=CGPoint.zero; var size=CGSize.zero
        AXValueGetValue(p as! AXValue,.cgPoint,&point); AXValueGetValue(s as! AXValue,.cgSize,&size)
        r["frame"]=[point.x,point.y,size.width,size.height]
    }
    rows.append(r)
    if depth>0 { for (i,c) in children(e).enumerated() { snapshot(c,path.isEmpty ? "\(i)" : "\(path).\(i)",depth-1,&rows) } }
}
do {
    let a=Array(CommandLine.arguments.dropFirst())
    guard AXIsProcessTrusted() else { throw NSError(domain:"Accessibility unavailable. Run from an already authorized Terminal session.",code:3) }
    switch a[0] {
    case "status":
        print("front",NSWorkspace.shared.frontmostApplication?.localizedName ?? "none","post",CGPreflightPostEventAccess())
    case "dump":
        var rows=[[String:Any]](); snapshot(try app(a[1]),"",Int(a.count>2 ? a[2]:"10")!,&rows)
        print(String(data:try JSONSerialization.data(withJSONObject:rows,options:[.sortedKeys]),encoding:.utf8)!)
    case "press":
        let e=try element(a[1],a[2]); let code=AXUIElementPerformAction(e,kAXPressAction as CFString)
        guard code == .success else { throw NSError(domain:"AXPress",code:Int(code.rawValue)) }
    case "set":
        let e=try element(a[1],a[2]); let code=AXUIElementSetAttributeValue(e,kAXValueAttribute as CFString,a[3] as CFString)
        guard code == .success else { throw NSError(domain:"AXValue",code:Int(code.rawValue)) }
    case "activate":
        NSWorkspace.shared.runningApplications.first(where:{$0.localizedName==a[1]})?.activate(options:[.activateIgnoringOtherApps])
    case "paste":
        NSPasteboard.general.clearContents(); NSPasteboard.general.setString(a[1],forType:.string)
        for down in [true,false] { let e=CGEvent(keyboardEventSource:nil,virtualKey:9,keyDown:down)!; e.flags = down ? .maskCommand : []; e.post(tap:.cghidEventTap); usleep(50000) }
    case "cgtext":
        let units=Array(a[1].utf16)
        for down in [true,false] { let e=CGEvent(keyboardEventSource:nil,virtualKey:0,keyDown:down)!; e.keyboardSetUnicodeString(stringLength:units.count,unicodeString:units); e.post(tap:.cghidEventTap); usleep(50000) }
    case "drag":
        let start=CGPoint(x:Double(a[1])!,y:Double(a[2])!), end=CGPoint(x:Double(a[3])!,y:Double(a[4])!)
        func post(_ type: CGEventType,_ pt:CGPoint) { let e=CGEvent(mouseEventSource:nil,mouseType:type,mouseCursorPosition:pt,mouseButton:.left)!; e.flags=[]; e.post(tap:.cghidEventTap) }
        post(.mouseMoved,start); usleep(200000); post(.leftMouseDown,start); usleep(200000)
        for i in 1...20 { let t=Double(i)/20; post(.leftMouseDragged,CGPoint(x:start.x+(end.x-start.x)*t,y:start.y+(end.y-start.y)*t)); usleep(30000) }
        post(.leftMouseUp,end)
    case "cgclick":
        let pt=CGPoint(x:Double(a[1])!,y:Double(a[2])!)
        let move=CGEvent(mouseEventSource:nil,mouseType:.mouseMoved,mouseCursorPosition:pt,mouseButton:.left)!; move.flags=[]; move.post(tap:.cghidEventTap); usleep(150000)
        for n in 1...(a.count>3 ? Int(a[3])! : 1) { for type in [CGEventType.leftMouseDown,CGEventType.leftMouseUp] { let e=CGEvent(mouseEventSource:nil,mouseType:type,mouseCursorPosition:pt,mouseButton:.left)!; var f=CGEventFlags(); for m in a.dropFirst(4) { if m=="ctrl" { f.insert(.maskControl) }; if m=="shift" { f.insert(.maskShift) }; if m=="cmd" { f.insert(.maskCommand) } }; e.flags=f; e.setIntegerValueField(.mouseEventClickState,value:Int64(n)); e.post(tap:.cghidEventTap); usleep(50000) } }
    case "cgkey":
        var flags=CGEventFlags()
        for m in a.dropFirst(2) { if m=="cmd" { flags.insert(.maskCommand) }; if m=="shift" { flags.insert(.maskShift) }; if m=="alt" { flags.insert(.maskAlternate) }; if m=="ctrl" { flags.insert(.maskControl) } }
        for down in [true,false] { let e=CGEvent(keyboardEventSource:nil,virtualKey:CGKeyCode(a[1])!,keyDown:down)!; e.flags=down ? flags : []; e.post(tap:.cghidEventTap); usleep(50000) }
    case "key":
        let names=["cmd":"command down","shift":"shift down","alt":"option down","ctrl":"control down"]
        let mods=a.dropFirst(2).compactMap { names[$0] }
        try osa("tell application \"System Events\" to key code \(Int(a[1])!)" + (mods.isEmpty ? "" : " using {"+mods.joined(separator:",")+"}"))
    case "text":
        let t=a[1].replacingOccurrences(of:"\\",with:"\\\\").replacingOccurrences(of:"\"",with:"\\\"")
        try osa("tell application \"System Events\" to keystroke \""+t+"\"")
    case "click":
        try osa("tell application \"System Events\" to click at {\(Int(a[1])!),\(Int(a[2])!)}")
    default: throw NSError(domain:"Unknown operation",code:4)
    }
} catch { fputs("\(error)\n",stderr); exit(1) }
