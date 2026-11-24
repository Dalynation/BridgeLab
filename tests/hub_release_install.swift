import Cocoa
import CryptoKit

// Exercise the actual Hub installer in a disposable home, including replacing
// a previous app. Never install into the real user's managed application folder.
@main struct ReleaseInstallCheck {
 static func main() throws {
  let archive=URL(fileURLWithPath:CommandLine.arguments[1])
  let version=CommandLine.arguments[2]
  let catalogURL=URL(fileURLWithPath:CommandLine.arguments[3])
  let catalog=try JSONDecoder().decode(Catalog.self,from:Data(contentsOf:catalogURL))
  try catalog.validate()
  let product=catalog.products.first{$0.id=="bridgelab"}!
  let home=FileManager.default.temporaryDirectory.appendingPathComponent("BridgeLab-release-install-"+UUID().uuidString)
  try FileManager.default.createDirectory(at:home,withIntermediateDirectories:true)
  defer {try? FileManager.default.removeItem(at:home)}
  let marker=home.appendingPathComponent("preserve-user-data.txt")
  try Data("preserved".utf8).write(to:marker)
  let baseline=URL(fileURLWithPath:CommandLine.arguments[4])
  var previousVersion:String?
  if product.isApplication(baseline) {
   previousVersion=product.installedVersion(at:baseline)
   let managed=product.managedApplication(home:home)
   try FileManager.default.createDirectory(at:managed.deletingLastPathComponent(),withIntermediateDirectories:true)
   let copy=Process();copy.executableURL=URL(fileURLWithPath:"/bin/cp");copy.arguments=["-cR",baseline.path,managed.path]
   try copy.run();copy.waitUntilExit();precondition(copy.terminationStatus==0)
  }
  let file=try FileHandle(forReadingFrom:archive);defer{try? file.close()}
  var hash=SHA256()
  while let chunk=try file.read(upToCount:1_048_576),!chunk.isEmpty{hash.update(data:chunk)}
  let digest=hash.finalize().map{String(format:"%02x",$0)}.joined()
  let size=(try FileManager.default.attributesOfItem(atPath:archive.path)[.size] as! NSNumber).int64Value
  let tag="v\(version)-preview",name="BridgeLab-\(version)-mac-preview.zip"
  let asset=ReleaseAsset(name:name,browser_download_url:"https://github.com/Dalynation/BridgeLab/releases/download/\(tag)/\(name)",size:size,digest:"sha256:"+digest)
  let release=Release(tag_name:tag,html_url:"https://github.com/Dalynation/BridgeLab/releases/tag/\(tag)",draft:false,prerelease:true,published_at:"release-check",assets:[asset])
  let installer=try PackageInstaller(product:product,release:release,home:home,readiness:{},progress:{_,_ in},completion:{_ in})
  let app=try installer.installLocalArchiveForTesting(archive)
  precondition(product.isApplication(app)&&product.supportsAuthentication(at:app))
  precondition(product.installedVersion(at:app)==version)
  let preserved=try String(contentsOf:marker,encoding:.utf8);precondition(preserved=="preserved")
  if let previousVersion {
   let backups=try FileManager.default.contentsOfDirectory(at:app.deletingLastPathComponent().appendingPathComponent("Backups"),includingPropertiesForKeys:nil)
   precondition(backups.contains{product.installedVersion(at:$0)==previousVersion})
  }
  print("PASS: Hub checksum, archive, extraction, signature, CPU/OS, version, authentication, managed install, data preservation and prior-app backup")
 }
}
