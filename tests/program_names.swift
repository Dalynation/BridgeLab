import Foundation
@main struct ProgramNamesTest {
 static func main()throws {
  let text="Wine diagnostic\r\nBridgeLabProgram 0 4669727374\r\nBridgeLabProgram 1 \r\nBridgeLabProgram 2 436166c3a90a42616e6b\r\n"
  let names=try BridgeManager.programNames(text)
  guard names==["First","Program 2","Café\nBank"] else{fatalError("Program names were not preserved")}
  for invalid in ["BridgeLabProgram 1 61","BridgeLabProgram 0 zz","BridgeLabProgram 0 6"] {
   do{_=try BridgeManager.programNames(invalid);fatalError("Malformed catalog accepted")}catch{}
  }
  guard try BridgeManager.programNames("BridgeLabProgram 0")==["Program 1"] else{fatalError("Trimmed empty name lost")}
  let cp1252=try BridgeManager.programNames("BridgeLabProgram 0 436166e9")
  guard cp1252==["Café"] else{fatalError("Windows preset encoding lost")}
  print("PASS: empty, UTF-8, Windows-1252 and multiline preset names; malformed records rejected")
 }
}
