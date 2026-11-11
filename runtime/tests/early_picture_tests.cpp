// "Low input lag": when a picture is shown early and when its flip presents it after all (host/early_picture.h).
#include "host/early_picture.h"
#include <cstdio>
#include <cstdlib>

namespace {
int failures=0;
void check(bool ok,const char* what) {if(!ok){std::printf("FAIL: %s\n",what);++failures;}}
EarlyPicture::Registers regs(std::uint64_t buffer) {EarlyPicture::Registers r{};r[9]=buffer;r[10]=0x1BF9FF0182271;return r;}
}

int main() {
    {
        // Double buffering: A, B, A, ... The picture after a flip goes out at its field's end with the registers of the
        // flip before, and the next flip presents nothing.
        EarlyPicture e;const auto a=regs(0),b=regs(0x40);
        check(!e.flipped(a,10,true) && !e.fieldEnded(10),"nothing early after one flip");
        check(!e.flipped(b,20,true) && !e.fieldEnded(20),"nothing early after two flips");
        check(!e.flipped(a,30,true),"the third flip presents its own picture");
        const auto next=e.fieldEnded(35);
        check(next && *next==b,"after A, B, A the next picture is shown with B's registers");
        check(!e.fieldEnded(35),"once per flip");
        check(e.flipped(b,35,true),"the flip that shows it presents nothing");
        const auto again=e.fieldEnded(40);
        check(again && *again==a,"and the one after with A's");
        check(e.flipped(a,40,true),"steady");
        check(e.shown_early==2 && e.shown_at_flip==3,"counts");
    }
    {
        // GS input after the early picture: the flip presents its own.
        EarlyPicture e;const auto a=regs(0),b=regs(0x40);
        e.flipped(a,1,true);e.flipped(b,2,true);e.flipped(a,3,true);
        check(e.fieldEnded(5).has_value(),"shown early");
        check(!e.flipped(b,6,true),"input since the early picture: the flip presents");
    }
    {
        // Registers other than predicted: the flip presents its own.
        EarlyPicture e;const auto a=regs(0),b=regs(0x40),c=regs(0x80);
        e.flipped(a,1,true);e.flipped(b,1,true);e.flipped(a,1,true);
        check(e.fieldEnded(1).has_value(),"shown early");
        check(!e.flipped(c,1,true),"another buffer: the flip presents");
        check(!e.fieldEnded(1),"no alternation (B, A, C): nothing early");
    }
    {
        // One buffer only (films that rewrite the display every field), or not allowed.
        EarlyPicture e;const auto a=regs(0),b=regs(0x40);
        e.flipped(a,1,true);e.flipped(a,1,true);e.flipped(a,1,true);
        check(!e.fieldEnded(1),"the same registers each flip: nothing early");
        e.flipped(b,1,true);e.flipped(a,1,false);
        check(!e.fieldEnded(1),"not allowed (fast forward, 60 fps fights): nothing early");
        e.flipped(b,1,true);
        check(e.fieldEnded(1).has_value(),"allowed again");
        e.cancel();
        check(!e.flipped(a,1,true),"cancelled: the flip presents");
    }
    {
        // A field without a flip shows nothing early.
        EarlyPicture e;const auto a=regs(0),b=regs(0x40);
        e.flipped(a,1,true);e.flipped(b,1,true);e.flipped(a,1,true);
        check(e.fieldEnded(1).has_value() && !e.fieldEnded(1),"only the flip's own field");
    }
    std::printf(failures?"early picture tests failed\n":"early picture tests passed\n");
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
