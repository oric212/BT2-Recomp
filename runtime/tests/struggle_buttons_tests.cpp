#include "ee/struggle_buttons.h"
#include <iostream>
#include <stdexcept>

// Beam struggles by buttons (ee/struggle_buttons.h): what a frame's presses push.
namespace {
void check(bool value,const char* what) {if(!value)throw std::runtime_error(what);}
constexpr std::uint32_t cross=1u<<14,square=1u<<15,triangle=1u<<12;
}

int main() {
    try {
        using namespace struggle_buttons;
        float owed=0;
        check(push(0,0,0.3f,owed)==0.3f && owed==0,"without presses the sticks' turn as measured");
        check(push(square,0,0,owed)==frame_turn && owed==press_turn-frame_turn,"a press pays at most a frame's turn");
        check(push(square,square,0,owed)==press_turn-frame_turn && owed==0,"holding pays the rest, nothing more");
        check(push(square,square,0,owed)==0,"a held button is not a new press");
        check(push(cross,0,0,owed)==0 && owed==0,"Cross does not push");
        owed=0;
        check(push(triangle,0,0,owed)==0 && owed==0,"other buttons do not push");
        check(push(square,0,1.6f,owed)==1.6f,"a larger stick turn wins");
        // Steady mashing at 7 presses a second (a press every fourth or fifth of the 30 frames) pays all it owes.
        owed=0;float total=0;
        for(unsigned frame=0;frame<30;++frame) {
            const bool press=frame*7/30!=(frame+1)*7/30;
            total+=push(press?square:0,0,0,owed);
        }
        check(total+owed>6.9f*press_turn && owed<press_turn,"mashing is paid out as it comes");
        std::cout<<"Struggle button checks passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
