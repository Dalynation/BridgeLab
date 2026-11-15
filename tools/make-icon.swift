import AppKit
let directory = CommandLine.arguments[1]
try FileManager.default.createDirectory(atPath: directory, withIntermediateDirectories: true)
for size in [16,32,64,128,256,512,1024] {
    let bitmap = NSBitmapImageRep(bitmapDataPlanes:nil,pixelsWide:size,pixelsHigh:size,bitsPerSample:8,samplesPerPixel:4,hasAlpha:true,isPlanar:false,colorSpaceName:.deviceRGB,bytesPerRow:0,bitsPerPixel:0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep:bitmap)!
    let ctx = NSGraphicsContext.current!.cgContext
    ctx.scaleBy(x:CGFloat(size)/1024,y:CGFloat(size)/1024)
    let outer = NSBezierPath(roundedRect:NSRect(x:40,y:40,width:944,height:944),xRadius:210,yRadius:210)
    NSColor(calibratedRed:0.06,green:0.08,blue:0.16,alpha:1).setFill();outer.fill()
    let gradient = NSGradient(starting:NSColor(calibratedRed:0.30,green:0.34,blue:0.96,alpha:1),ending:NSColor(calibratedRed:0.10,green:0.82,blue:0.80,alpha:1))!
    let bridge = NSBezierPath()
    bridge.move(to:NSPoint(x:230,y:330));bridge.line(to:NSPoint(x:230,y:525))
    bridge.curve(to:NSPoint(x:794,y:525),controlPoint1:NSPoint(x:230,y:800),controlPoint2:NSPoint(x:794,y:800))
    bridge.line(to:NSPoint(x:794,y:330));bridge.lineWidth=92;bridge.lineCapStyle = .round
    NSColor.white.setStroke();bridge.stroke()
    let deck=NSBezierPath(roundedRect:NSRect(x:150,y:320,width:724,height:82),xRadius:41,yRadius:41)
    gradient.draw(in:deck,angle:0)
    for (x,h) in [(390,120),(512,210),(634,120)] {
        let bar=NSBezierPath(roundedRect:NSRect(x:x-24,y:440,width:48,height:h),xRadius:24,yRadius:24)
        gradient.draw(in:bar,angle:90)
    }
    NSGraphicsContext.restoreGraphicsState()
    let data=bitmap.representation(using:.png,properties:[:])!
    let filename = size == 1024 ? "icon_512x512@2x.png" : "icon_\(size)x\(size).png"
    try data.write(to:URL(fileURLWithPath:directory).appendingPathComponent(filename))
}
// Supply Retina variants in the iconset's standard filenames.
for size in [16,32,128,256] {
    let source=URL(fileURLWithPath:directory).appendingPathComponent("icon_\(size*2)x\(size*2).png")
    let dest=URL(fileURLWithPath:directory).appendingPathComponent("icon_\(size)x\(size)@2x.png")
    try? FileManager.default.removeItem(at:dest)
    try FileManager.default.copyItem(at:source,to:dest)
}

try? FileManager.default.removeItem(atPath: directory + "/icon_64x64.png")
