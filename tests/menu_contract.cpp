#include <windows.h>
#include <iostream>
#include "../src/editor_popups.hpp"
#include "../src/editor_menu.hpp"
static HMENU menu=nullptr,submenu=nullptr;
static EditorPopups popups;
static int scenario=0,stage=0;
static DWORD started=0;
static bool nestedLayout=true;
static HWND popupWindow(POINT screen){for(auto& p:popups.windows)if(PtInRect(&p.source,screen))return p.window;return nullptr;}
static bool point(HMENU m,int index,POINT& p){RECT r{};if(!GetMenuItemRect(nullptr,m,index,&r))return false;p={(r.left+r.right)/2,(r.top+r.bottom)/2};return true;}
static void click(HWND window,POINT p){EditorMenu::mouse(window,WM_MOUSEMOVE,0,p);EditorMenu::mouse(window,WM_LBUTTONDOWN,MK_LBUTTON,p);EditorMenu::mouse(window,WM_LBUTTONUP,0,p);}
static LRESULT CALLBACK proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_TIMER){
  popups.refresh(window);
  if(GetTickCount()-started>5000){EndMenu();return 0;}
  if(scenario==5&&stage==0&&EditorMenu::active()){
   stage=1;EditorMenu::key(EditorMenu::active(),WM_KEYDOWN,VK_ESCAPE);return 0;
  }
  if(popups.windows.empty())return 0;
  POINT p{};
  if(scenario==1&&stage==0&&point(menu,1,p)){stage=1;click(popupWindow(p),p);}
  if(scenario==2){
   if(stage==0&&point(menu,0,p)){stage=1;EditorMenu::mouse(popupWindow(p),WM_MOUSEMOVE,0,p);}
   else if(stage==1&&popups.windows.size()>1&&point(submenu,1,p)){
    stage=2;const EditorPopups::Popup* parent=nullptr;const EditorPopups::Popup* child=nullptr;
    for(const auto& popup:popups.windows){if(EditorPopups::menu(popup.window)==menu)parent=&popup;if(EditorPopups::menu(popup.window)==submenu)child=&popup;}
    RECT overlap{};nestedLayout=parent&&child&&!IntersectRect(&overlap,&parent->destination,&child->destination)&&popups.windows.front().window==child->window;
    click(popupWindow(p),p);
   }
  }
  if(scenario==3&&stage==0){stage=1;EditorMenu::key(popups.windows.front().window,WM_KEYDOWN,VK_ESCAPE);}
  if(scenario==4&&stage==0){stage=1;POINT outside{-12500,-12500};click(popups.windows.front().window,outside);}
  return 0;
 }
 return DefWindowProcW(window,message,wp,lp);
}
int main(){
 WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"BridgeLabMenuContract";RegisterClassW(&cls);
 HWND root=CreateWindowW(cls.lpszClassName,L"QA",WS_OVERLAPPEDWINDOW|WS_VISIBLE,-12000,-12000,400,300,nullptr,nullptr,cls.hInstance,nullptr);
 SetTimer(root,1,30,nullptr);popups.anchor={30,30};bool passed=true;
 for(scenario=1;scenario<=5;++scenario){
  menu=CreatePopupMenu();submenu=nullptr;
  if(scenario==5)for(int i=0;i<80;++i)AppendMenuW(menu,MF_STRING,300+i,L"Oversized menu item");
  if(scenario==2){submenu=CreatePopupMenu();AppendMenuW(submenu,MF_STRING,201,L"First child");AppendMenuW(submenu,MF_STRING,202,L"Second child");AppendMenuW(menu,MF_POPUP,(UINT_PTR)submenu,L"Submenu");}
  AppendMenuW(menu,MF_STRING,101,L"First");AppendMenuW(menu,MF_STRING,102,L"Second");stage=0;started=GetTickCount();
  UINT selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,-12000,-12000,0,root,nullptr);
  UINT expected=scenario==1?102:scenario==2?202:0;
  bool ok=selected==expected&&stage==(scenario==2?2:1)&&GetTickCount()-started<5000;
  passed=passed&&ok;std::cout<<(ok?"PASS":"FAIL")<<": native menu scenario "<<scenario<<" selected="<<selected<<" stage="<<stage<<"\n";
  DestroyMenu(menu);
 }
 std::cout<<(nestedLayout?"PASS":"FAIL")<<": submenu remains beside and above its parent\n";passed=passed&&nestedLayout;
 EditorMenu::reset();passed=passed&&!EditorMenu::hook&&EditorMenu::pending.empty();DestroyWindow(root);
 return passed?0:1;
}
