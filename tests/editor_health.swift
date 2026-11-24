import Foundation
@main struct HealthChecks {
 static func main() throws {
  let manager=BridgeManager(),fm=FileManager.default
  let directory=URL(fileURLWithPath:CommandLine.arguments[1]).appendingPathComponent(UUID().uuidString)
  try fm.createDirectory(at:directory,withIntermediateDirectories:true)
  defer{try? fm.removeItem(at:directory)}
  let mapping=directory.appendingPathComponent("BridgeLab-editor-test")
  fm.createFile(atPath:mapping.path,contents:nil)
  let file=try FileHandle(forWritingTo:mapping);try file.truncate(atOffset:UInt64(16+2048*2048*4+4));try file.close()
  let status=mapping.appendingPathExtension("health.json")
  try manager.writeJSON(["format_version":1,"unsupported_popup_modes":1,"capture_failure":0],status)
  let observations=manager.editorHealth(in:directory)
  precondition(observations.count==1)
  precondition(manager.editorHealthMessage(observations).contains("cannot yet forward"))
  precondition(manager.editorHealthMessage([["input_rejected":1]]).contains("could not reach"))
  precondition(manager.editorHealthMessage([["frame_stalled":1]]).contains("stopped updating"))
  precondition(manager.editorHealthMessage([["quiet_while_notes":1]]).contains("can be intentional"))
  precondition(manager.editorHealthMessage([["nonfinite_audio_samples":1]]).contains("invalid audio"))
  precondition(manager.editorHealthMessage([["above_full_scale_recent":1]]).contains("headroom"))
  precondition(manager.editorHealthMessage([["audio_blocks_observed":1,"note_ons_observed":0]]).contains("no note-on events"))
  let produced=manager.editorHealthMessage([["instrument_name":"Test instrument","audio_blocks_observed":100,"audio_rms":0.1,"note_ons_observed":1]])
  precondition(produced.contains("Test instrument: audio is being produced"))
  let stale=manager.editorHealthMessage([["quiet_while_notes":1,"audio_observation_stale":1]])
  precondition(stale.contains("no recent audio blocks") && !stale.contains("is very quiet"))
  precondition(!manager.editorHealthMessage([]).contains("verified successfully"))
  try fm.removeItem(at:mapping)
  precondition(manager.editorHealth(in:directory).isEmpty,"Orphan status must not count as a live editor")
  fm.createFile(atPath:mapping.path,contents:Data([0]))
  precondition(manager.editorHealth(in:directory).isEmpty,"Invalid frame size must be rejected")
  print("Popup health: active mapping validation, unsupported-mode warning, orphan/invalid rejection: PASS")
 }
}
