// The memory card folder's file layer on a synthetic card: opening, reading, seeking, listing,
// the current directory, result codes, and that nothing outside the folder can be named.
#include "services/card_folder.h"
#include <cstdlib>
#include <iostream>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Card folder check failed: "<<what<<'\n';std::exit(1);}}
void put(const std::filesystem::path& path,const std::string& content) {std::ofstream(path,std::ios::binary)<<content;}
}

int main() {
    const auto root=std::filesystem::temp_directory_path()/"bt2-card-folder-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root/"card"/"SAVE-ONE");
    std::filesystem::create_directories(root/"card"/"SAVE-TWO");
    put(root/"outside.txt","not on the card");
    put(root/"card"/"SAVE-ONE"/"data","0123456789");
    put(root/"card"/"SAVE-ONE"/"icon.sys","icon");
    put(root/"card"/"SAVE-ONE"/"entries.txt",". 8427 0102030405060708 1112131415161718\ndata 8417 2122232425262728 3132333435363738\nbroken line\n");
    CardFolder card(root/"card");

    // Reading a file by its full path, in pieces, with seeks.
    const int fd=card.open("/SAVE-ONE/data",1);
    check(fd==0,"the first file gets descriptor 0");
    std::array<std::uint8_t,4> part{};
    check(card.read(fd,part)==4 && part[0]=='0' && part[3]=='3',"the first bytes");
    check(card.seek(fd,2,1)==6 && card.read(fd,part)==4 && part[0]=='6' && part[3]=='9',"a seek from the position");
    check(card.read(fd,part)==0,"nothing at the end");
    check(card.seek(fd,-3,2)==7 && card.read(fd,part)==3 && part[0]=='7',"a seek from the end; a short read");
    check(card.seek(fd,0,0)==0 && card.seek(fd,11,0)==CardFolder::denied && card.seek(fd,-1,0)==CardFolder::denied,"seeks stay inside the file");
    check(card.close(fd)==0 && card.close(fd)==CardFolder::no_entry && card.read(fd,part)==CardFolder::no_entry,"a closed descriptor is gone");

    // Result codes: missing files, folders, writing, too many files, the metadata file, leaving the card.
    check(card.open("/SAVE-ONE/none",1)==CardFolder::no_entry && card.open("/SAVE-ONE",1)==CardFolder::no_entry,"no such file; a folder is not a file");
    check(card.open("/SAVE-ONE/data",2)==CardFolder::denied && card.open("/SAVE-ONE/data",0x203)==CardFolder::denied,"writing is refused");
    check(card.open("/SAVE-ONE/entries.txt",1)==CardFolder::no_entry,"the metadata file is not on the card");
    check(card.open("/../outside.txt",1)==CardFolder::no_entry && card.open("/SAVE-ONE/../../outside.txt",1)==CardFolder::no_entry,"nothing outside the card");
    check(card.open("/SAVE-ONE/da:ta",1)==CardFolder::no_entry,"characters a card name cannot have");
    check(card.open("/SAVE-ONE/data",1)==0 && card.open("/SAVE-ONE/data",1)==1 && card.open("/SAVE-ONE/icon.sys",1)==2,"three files at once");
    check(card.open("/SAVE-ONE/data",1)==CardFolder::too_many_open,"not a fourth");
    for(int n=0;n<3;++n)card.close(n);

    // Listing: the top, a save's folder with "." and ".." first, a single entry, modes and time stamps.
    const auto top=card.list("/*");
    check(top.size()==2 && top[0].name=="SAVE-ONE" && top[1].name=="SAVE-TWO" && top[0].mode==0x8427 && top[0].created[0]==1 && top[0].modified[7]==0x18,"the saves on the card, with the folder's own entry data");
    check(top[1].mode==CardFolder::folder_mode && top[1].created[0]==0,"defaults without metadata");
    const auto inside=card.list("/SAVE-ONE/*");
    check(inside.size()==4 && inside[0].name=="." && inside[1].name==".." && inside[2].name=="data" && inside[3].name=="icon.sys","a save's files after . and ..");
    check(inside[2].size==10 && inside[2].mode==0x8417 && inside[2].created[0]==0x21 && inside[2].modified[7]==0x38,"a file's size, mode and time stamps");
    check(inside[3].size==4 && inside[3].mode==CardFolder::file_mode,"a file without metadata");
    const auto one=card.list("/SAVE-ONE/data");
    check(one.size()==1 && one[0].name=="data" && card.list("/SAVE-ONE/none").empty() && card.list("/NONE/*").empty(),"a single entry; nothing for what is not there");

    // The current directory: relative names follow it.
    check(card.currentDirectory()=="/" && card.changeDirectory("/SAVE-ONE")==0 && card.currentDirectory()=="/SAVE-ONE","into a save's folder");
    const int relative=card.open("data",1);
    check(relative==0 && card.list("*").size()==4,"relative names");
    card.close(relative);
    check(card.changeDirectory("..")==0 && card.currentDirectory()=="/" && card.changeDirectory("..")==CardFolder::no_entry,"up to the top and no further");
    check(card.changeDirectory("/SAVE-ONE/data")==CardFolder::no_entry && card.changeDirectory("/NONE")==CardFolder::no_entry,"only folders");

    std::filesystem::remove_all(root);
    std::cout<<"Card folder checks passed.\n";
}
