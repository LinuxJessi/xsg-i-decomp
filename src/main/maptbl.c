#include "common.h"
#include "maptbl.h"

/* MapFileNameGet indexes group.names by map ID and reads group.count. */


extern const char D_004BE338[];
extern const char D_004BE348[];
extern const char D_004BE358[];
extern const char D_004BE368[];
extern const char D_004BE378[];
extern const char D_004BE388[];
extern const char D_004BE398[];
extern const char D_004BE3A8[];
extern const char D_004BE3B8[];
extern const char D_004BE3C8[];
extern const char D_004BE3D8[];
extern const char D_004BE3E8[];
extern const char D_004BE3F8[];
extern const char D_004BE408[];
extern const char D_004BE418[];
extern const char D_004BE428[];
extern const char D_004BE438[];
extern const char D_004BE448[];
extern const char D_004BE458[];
extern const char D_004BE468[];
extern const char D_004BE478[];
extern const char D_004BE488[];
extern const char D_004BE498[];
extern const char D_004BE4A8[];
extern const char D_004BE4B8[];
extern const char D_004BE4C8[];
extern const char D_004BE4D8[];
extern const char D_004BE4E8[];
extern const char D_004BE4F8[];
extern const char D_004BE508[];
extern const char D_004BE518[];
extern const char D_004BE528[];
extern const char D_004BE538[];
extern const char D_004BE548[];
extern const char D_004BE558[];
extern const char D_004BE568[];
extern const char D_004BE578[];
extern const char D_004BE588[];
extern const char D_004BE598[];
extern const char D_004BE5A8[];
extern const char D_004BE5B8[];
extern const char D_004BE5C8[];
extern const char D_004BE5D8[];
extern const char D_004BE5E8[];
extern const char D_004BE5F8[];
extern const char D_004BE608[];
extern const char D_004BE618[];
extern const char D_004BE628[];
extern const char D_004BE638[];
extern const char D_004BE648[];
extern const char D_004BE658[];
extern const char D_004BE668[];
extern const char D_004BE678[];
extern const char D_004BE688[];
extern const char D_004BE698[];
extern const char D_004BE6A8[];
extern const char D_004BE6B8[];
extern const char D_004BE6C8[];
extern const char D_004BE6D8[];
extern const char D_004BE6E8[];
extern const char D_004BE6F8[];
extern const char D_004BE708[];
extern const char D_004BE718[];
extern const char D_004BE728[];
extern const char D_004BE738[];
extern const char D_004BE748[];
extern const char D_004BE758[];
extern const char D_004BE768[];
extern const char D_004BE778[];
extern const char D_004BE788[];
extern const char D_004BE798[];
extern const char D_004BE7A8[];
extern const char D_004BE7B8[];
extern const char D_004BE7C8[];
extern const char D_004BE7D8[];
extern const char D_004BE7E8[];
extern const char D_004BE7F8[];
extern const char D_004BE808[];
extern const char D_004BE818[];
extern const char D_004BE828[];
extern const char D_004BE838[];
extern const char D_004BE848[];
extern const char D_004BE858[];
extern const char D_004BE868[];
extern const char D_004BE878[];
extern const char D_004BE888[];
extern const char D_004BE898[];
extern const char D_004BE8A8[];
extern const char D_004BE8B8[];
extern const char D_004BE8C8[];
extern const char D_004BE8D8[];
extern const char D_004BE8E8[];
extern const char D_004BE8F8[];
extern const char D_004BE908[];
extern const char D_004BE918[];
extern const char D_004BE928[];
extern const char D_004BE938[];
extern const char D_004BE948[];
extern const char D_004BE958[];
extern const char D_004BE968[];
extern const char D_004BE978[];
extern const char D_004BE988[];
extern const char D_004BE998[];
extern const char D_004BE9A8[];
extern const char D_004BE9B8[];
extern const char D_004BE9C8[];
extern const char D_004BE9D8[];
extern const char D_004BE9E8[];
extern const char D_004BE9F8[];
extern const char D_004BEA08[];
extern const char D_004BEA18[];
extern const char D_004BEA28[];
extern const char D_004BEA38[];
extern const char D_004BEA48[];
extern const char D_004BEA58[];
extern const char D_004BEA68[];
extern const char D_004BEA78[];
extern const char D_004BEA88[];
extern const char D_004BEA98[];
extern const char D_004BEAA8[];
extern const char D_004BEAB8[];
extern const char D_004BEAC8[];
extern const char D_004BEAD8[];
extern const char D_004BEAE8[];
extern const char D_004BEAF8[];
extern const char D_004BEB08[];
extern const char D_004BEB18[];
extern const char D_004BEB28[];
extern const char D_004BEB38[];
extern const char D_004BEB48[];
extern const char D_004BEB58[];
extern const char D_004BEB68[];
extern const char D_004BEB78[];
extern const char D_004BEB88[];
extern const char D_004BEB98[];
extern const char D_004BEBA8[];
extern const char D_004BEBB8[];
extern const char D_004BEBC8[];
extern const char D_004BEBD8[];
extern const char D_004BEBE8[];
extern const char D_004BEBF8[];
extern const char D_004BEC08[];
extern const char D_004BEC18[];
extern const char D_004BEC28[];
extern const char D_004BEC38[];
extern const char D_004BEC48[];
extern const char D_004BEC58[];
extern const char D_004BEC68[];
extern const char D_004BEC78[];
extern const char D_004BEC88[];
extern const char D_004BEC98[];
extern const char D_004BECA8[];
extern const char D_004BECB8[];
extern const char D_004BECC8[];
extern const char D_004BECD8[];
extern const char D_004BECE8[];
extern const char D_004BECF8[];
extern const char D_004BED08[];
extern const char D_004BED18[];
extern const char D_004BED28[];
extern const char D_004BED38[];
extern const char D_004BED48[];
extern const char D_004BED58[];
extern const char D_004BED68[];
extern const char D_004BED78[];
extern const char D_004BED88[];
extern const char D_004BED98[];
extern const char D_004BEDA8[];
extern const char D_004BEDB8[];
extern const char D_004BEDC8[];
extern const char D_004BEDD8[];
extern const char D_004BEDE8[];
extern const char D_004BEDF8[];
extern const char D_004BEE08[];
extern const char D_004BEE18[];
extern const char D_004BEE28[];
extern const char D_004BEE38[];
extern const char D_004BEE48[];
extern const char D_004BEE58[];
extern const char D_004BEE68[];
extern const char D_004BEE78[];
extern const char D_004BEE88[];
extern const char D_004BEE98[];
extern const char D_004BEEA8[];
extern const char D_004BEEB8[];
extern const char D_004BEEC8[];
extern const char D_004BEED8[];
extern const char D_004BEEE8[];
extern const char D_004BEEF8[];
extern const char D_004BEF08[];
extern const char D_004BEF18[];
extern const char D_004BEF28[];
extern const char D_004BEF38[];
extern const char D_004BEF48[];
extern const char D_004BEF58[];
extern const char D_004BEF68[];
extern const char D_004BEF78[];
extern const char D_004BEF88[];
extern const char D_004BEF98[];
extern const char D_004BEFA8[];
extern const char D_004BEFB8[];
extern const char D_004BEFC8[];
extern const char D_004BEFD8[];
extern const char D_004BEFE8[];
extern const char D_004BEFF8[];
extern const char D_004BF008[];
extern const char D_004BF018[];
extern const char D_004BF028[];
extern const char D_004BF038[];
extern const char D_004BF048[];
extern const char D_004BF058[];
extern const char D_004BF068[];
extern const char D_004BF078[];
extern const char D_004BF088[];
extern const char D_004BF098[];
extern const char D_004BF0A8[];
extern const char D_004BF0B8[];
extern const char D_004BF0C8[];
extern const char D_004BF0D8[];
extern const char D_004BF0E8[];
extern const char D_004BF0F8[];
extern const char D_004BF108[];
extern const char D_004BF118[];
extern const char D_004BF128[];
extern const char D_004BF138[];
extern const char D_004BF148[];
extern const char D_004BF158[];
extern const char D_004BF168[];
extern const char D_004BF178[];
extern const char D_004BF188[];
extern const char D_004BF198[];
extern const char D_004BF1A8[];
extern const char D_004BF1B8[];
extern const char D_004BF1C8[];
extern const char D_004BF1D8[];
extern const char D_004BF1E8[];
extern const char D_004BF1F8[];
extern const char D_004BF208[];
extern const char D_004BF218[];
extern const char D_004BF228[];
extern const char D_004BF238[];
extern const char D_004BF248[];
extern const char D_004BF258[];
extern const char D_004BF268[];
extern const char D_004BF278[];
extern const char D_004BF288[];
extern const char D_004BF298[];
extern const char D_004BF2A8[];
extern const char D_004BF2B8[];
extern const char D_004BF2C8[];
extern const char D_004BF2D8[];
extern const char D_004BF2E8[];
extern const char D_004BF2F8[];
extern const char D_004BF308[];
extern const char D_004BF318[];
extern const char D_004BF328[];
extern const char D_004BF338[];
extern const char D_004BF348[];
extern const char D_004BF358[];
extern const char D_004BF368[];
extern const char D_004BF378[];
extern const char D_004BF388[];
extern const char D_004BF398[];
extern const char D_004BF3A8[];
extern const char D_004BF3B8[];
extern const char D_004BF3C8[];
extern const char D_004BF3D8[];
extern const char D_004BF3E8[];
extern const char D_004BF3F8[];
extern const char D_004BF408[];
extern const char D_004BF418[];
extern const char D_004BF428[];
extern const char D_004BF438[];
extern const char D_004BF448[];
extern const char D_004BF458[];
extern const char D_004BF468[];
extern const char D_004BF478[];
extern const char D_004BF488[];
extern const char D_004BF498[];
extern const char D_004BF4A8[];
extern const char D_004BF4B8[];
extern const char D_004BF4C8[];
extern const char D_004BF4D8[];
extern const char D_004BF4E8[];
extern const char D_004BF4F8[];
extern const char D_004BF508[];
extern const char D_004BF518[];
extern const char D_004BF528[];
extern const char D_004BF538[];
extern const char D_004BF548[];
extern const char D_004BF558[];
extern const char D_004BF568[];
extern const char D_004BF578[];
extern const char D_004BF588[];
extern const char D_004BF598[];
extern const char D_004BF5A8[];
extern const char D_004BF5B8[];
extern const char D_004BF5C8[];
extern const char D_004BF5D8[];
extern const char D_004BF5E8[];
extern const char D_004BF5F8[];
extern const char D_004BF608[];
extern const char D_004BF618[];
extern const char D_004BF628[];
extern const char D_004BF638[];
extern const char D_004BF648[];
extern const char D_004BF658[];
extern const char D_004BF668[];
extern const char D_004BF678[];
extern const char D_004BF688[];
extern const char D_004BF698[];
extern const char D_004BF6A8[];
extern const char D_004BF6B8[];
extern const char D_004BF6C8[];
extern const char D_004BF6D8[];
extern const char D_004BF6E8[];
extern const char D_004BF6F8[];
extern const char D_004BF708[];
extern const char D_004BF718[];
extern const char D_004BF728[];
extern const char D_004BF738[];
extern const char D_004D8A80[];
extern const char D_004D8A88[];

static char *maptbl0000[58] = {
    (char *)D_004BE668,
    (char *)D_004BE658,
    (char *)D_004BE648,
    (char *)D_004BE638,
    (char *)D_004BE628,
    (char *)D_004BE618,
    (char *)D_004BE608,
    (char *)D_004BE5F8,
    (char *)D_004BE5E8,
    (char *)D_004BE5D8,
    (char *)D_004BE5C8,
    (char *)D_004BE5B8,
    (char *)D_004BE5A8,
    (char *)D_004BE598,
    (char *)D_004D8A88,
    (char *)D_004BE588,
    (char *)D_004BE578,
    (char *)D_004BE568,
    (char *)D_004BE558,
    (char *)D_004BE548,
    (char *)D_004BE538,
    (char *)D_004BE528,
    (char *)D_004BE518,
    (char *)D_004BE508,
    (char *)D_004BE4F8,
    (char *)D_004BE4E8,
    (char *)D_004BE4D8,
    (char *)D_004BE4C8,
    (char *)D_004BE4B8,
    (char *)D_004BE4A8,
    (char *)D_004BE498,
    (char *)D_004BE488,
    (char *)D_004D8A80,
    (char *)D_004BE478,
    (char *)D_004BE468,
    (char *)D_004BE458,
    (char *)D_004BE448,
    (char *)D_004BE438,
    (char *)D_004BE428,
    (char *)D_004BE418,
    (char *)D_004BE408,
    (char *)D_004BE3F8,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004BE3E8,
    (char *)D_004BE3D8,
    (char *)D_004BE3C8,
    (char *)D_004BE3B8,
    (char *)D_004BE3A8,
    (char *)D_004BE398,
    (char *)D_004BE388,
    (char *)D_004BE378,
    (char *)D_004BE368,
    (char *)D_004BE358,
    (char *)D_004BE348,
    (char *)D_004BE338
};

static char *maptbl0100[32] = {
    (char *)D_004BE3E8,
    (char *)D_004BE7E8,
    (char *)D_004BE7D8,
    (char *)D_004BE7C8,
    (char *)D_004BE7B8,
    (char *)D_004BE7A8,
    (char *)D_004BE798,
    (char *)D_004BE788,
    (char *)D_004BE778,
    (char *)D_004BE768,
    (char *)D_004BE758,
    (char *)D_004BE748,
    (char *)D_004BE738,
    (char *)D_004D8A80,
    (char *)D_004BE728,
    (char *)D_004BE718,
    (char *)D_004BE708,
    (char *)D_004BE6F8,
    (char *)D_004BE6E8,
    (char *)D_004BE6D8,
    (char *)D_004BE6C8,
    (char *)D_004BE6B8,
    (char *)D_004BE6A8,
    (char *)D_004BE698,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004BE688,
    (char *)D_004BE678
};

/* Original group 02 has count zero and a genuine zero-size LOCAL OBJECT.
 * Its empty table shares the following group03 table address; no storage
 * or new DATA credit is assigned to this declaration. */
static char *maptbl0200[0] = {};

static char *maptbl0300[1] = {
    (char *)D_004BE7F8
};

static char *maptbl0400[2] = {
    (char *)D_004BE818,
    (char *)D_004BE808
};

static char *maptbl0500[8] = {
    (char *)D_004BE898,
    (char *)D_004BE888,
    (char *)D_004BE878,
    (char *)D_004BE868,
    (char *)D_004BE858,
    (char *)D_004BE848,
    (char *)D_004BE838,
    (char *)D_004BE828
};

static char *maptbl0600[2] = {
    (char *)D_004BE8B8,
    (char *)D_004BE8A8
};

static char *maptbl0700[48] = {
    (char *)D_004BEBB8,
    (char *)D_004BEBA8,
    (char *)D_004BEB98,
    (char *)D_004BEB88,
    (char *)D_004BEB78,
    (char *)D_004BEB68,
    (char *)D_004BEB58,
    (char *)D_004BEB48,
    (char *)D_004BEB38,
    (char *)D_004BEB28,
    (char *)D_004BEB18,
    (char *)D_004BEB08,
    (char *)D_004BEAF8,
    (char *)D_004BEAE8,
    (char *)D_004BEAD8,
    (char *)D_004BEAC8,
    (char *)D_004BEAB8,
    (char *)D_004BEAA8,
    (char *)D_004BEA98,
    (char *)D_004BEA88,
    (char *)D_004BEA78,
    (char *)D_004BEA68,
    (char *)D_004BEA58,
    (char *)D_004BEA48,
    (char *)D_004BEA38,
    (char *)D_004BEA28,
    (char *)D_004BEA18,
    (char *)D_004BEA08,
    (char *)D_004BE9F8,
    (char *)D_004BE9E8,
    (char *)D_004BE9D8,
    (char *)D_004BE9C8,
    (char *)D_004BE9B8,
    (char *)D_004BE9A8,
    (char *)D_004BE998,
    (char *)D_004BE988,
    (char *)D_004BE978,
    (char *)D_004BE968,
    (char *)D_004BE958,
    (char *)D_004BE948,
    (char *)D_004BE938,
    (char *)D_004BE928,
    (char *)D_004BE918,
    (char *)D_004BE908,
    (char *)D_004BE8F8,
    (char *)D_004BE8E8,
    (char *)D_004BE8D8,
    (char *)D_004BE8C8
};

static char *maptbl0800[19] = {
    (char *)D_004BECC8,
    (char *)D_004BECB8,
    (char *)D_004BECA8,
    (char *)D_004BEC98,
    (char *)D_004BEC88,
    (char *)D_004BEC78,
    (char *)D_004BEC68,
    (char *)D_004BEC58,
    (char *)D_004D8A80,
    (char *)D_004BEC48,
    (char *)D_004D8A80,
    (char *)D_004BEC38,
    (char *)D_004BEC28,
    (char *)D_004BEC18,
    (char *)D_004BEC08,
    (char *)D_004BEBF8,
    (char *)D_004BEBE8,
    (char *)D_004BEBD8,
    (char *)D_004BEBC8
};

static char *maptbl0900[19] = {
    (char *)D_004BEDF8,
    (char *)D_004BEDE8,
    (char *)D_004BEDD8,
    (char *)D_004BEDC8,
    (char *)D_004BEDB8,
    (char *)D_004BEDA8,
    (char *)D_004BED98,
    (char *)D_004BED88,
    (char *)D_004BED78,
    (char *)D_004BED68,
    (char *)D_004BED58,
    (char *)D_004BED48,
    (char *)D_004BED38,
    (char *)D_004BED28,
    (char *)D_004BED18,
    (char *)D_004BED08,
    (char *)D_004BECF8,
    (char *)D_004BECE8,
    (char *)D_004BECD8
};

static char *maptbl1000[1] = {
    (char *)D_004BEE08
};

static char *maptbl1100[9] = {
    (char *)D_004BEE98,
    (char *)D_004BEE88,
    (char *)D_004BEE78,
    (char *)D_004BEE68,
    (char *)D_004BEE58,
    (char *)D_004BEE48,
    (char *)D_004BEE38,
    (char *)D_004BEE28,
    (char *)D_004BEE18
};

static char *maptbl1200[41] = {
    (char *)D_004BF128,
    (char *)D_004BF118,
    (char *)D_004BF108,
    (char *)D_004BF0F8,
    (char *)D_004BF0E8,
    (char *)D_004BF0D8,
    (char *)D_004BF0C8,
    (char *)D_004BF0B8,
    (char *)D_004BF0A8,
    (char *)D_004BF098,
    (char *)D_004BF088,
    (char *)D_004BF078,
    (char *)D_004BF068,
    (char *)D_004BF058,
    (char *)D_004BF048,
    (char *)D_004BF038,
    (char *)D_004BF028,
    (char *)D_004BF018,
    (char *)D_004BF008,
    (char *)D_004BEFF8,
    (char *)D_004BEFE8,
    (char *)D_004BEFD8,
    (char *)D_004BEFC8,
    (char *)D_004BEFB8,
    (char *)D_004BEFA8,
    (char *)D_004BEF98,
    (char *)D_004BEF88,
    (char *)D_004BEF78,
    (char *)D_004BEF68,
    (char *)D_004BEF58,
    (char *)D_004BEF48,
    (char *)D_004BEF38,
    (char *)D_004BEF28,
    (char *)D_004BEF18,
    (char *)D_004BEF08,
    (char *)D_004BEEF8,
    (char *)D_004BEEE8,
    (char *)D_004BEED8,
    (char *)D_004BEEC8,
    (char *)D_004BEEB8,
    (char *)D_004BEEA8
};

static char *maptbl1300[24] = {
    (char *)D_004BF2A8,
    (char *)D_004BF298,
    (char *)D_004BF288,
    (char *)D_004BF278,
    (char *)D_004BF268,
    (char *)D_004BF258,
    (char *)D_004BF248,
    (char *)D_004BF238,
    (char *)D_004BF228,
    (char *)D_004BF218,
    (char *)D_004BF208,
    (char *)D_004BF1F8,
    (char *)D_004BF1E8,
    (char *)D_004BF1D8,
    (char *)D_004BF1C8,
    (char *)D_004BF1B8,
    (char *)D_004BF1A8,
    (char *)D_004BF198,
    (char *)D_004BF188,
    (char *)D_004BF178,
    (char *)D_004BF168,
    (char *)D_004BF158,
    (char *)D_004BF148,
    (char *)D_004BF138
};

static char *maptbl1400[25] = {
    (char *)D_004BF438,
    (char *)D_004BF428,
    (char *)D_004BF418,
    (char *)D_004BF408,
    (char *)D_004BF3F8,
    (char *)D_004BF3E8,
    (char *)D_004BF3D8,
    (char *)D_004BF3C8,
    (char *)D_004BF3B8,
    (char *)D_004BF3A8,
    (char *)D_004BF398,
    (char *)D_004BF388,
    (char *)D_004BF378,
    (char *)D_004BF368,
    (char *)D_004BF358,
    (char *)D_004BF348,
    (char *)D_004BF338,
    (char *)D_004BF328,
    (char *)D_004BF318,
    (char *)D_004BF308,
    (char *)D_004BF2F8,
    (char *)D_004BF2E8,
    (char *)D_004BF2D8,
    (char *)D_004BF2C8,
    (char *)D_004BF2B8
};

static char *maptbl1500[12] = {
    (char *)D_004BF4F8,
    (char *)D_004BF4E8,
    (char *)D_004BF4D8,
    (char *)D_004BF4C8,
    (char *)D_004BF4B8,
    (char *)D_004BF4A8,
    (char *)D_004BF498,
    (char *)D_004BF488,
    (char *)D_004BF478,
    (char *)D_004BF468,
    (char *)D_004BF458,
    (char *)D_004BF448
};

static char *maptbl1600[7] = {
    (char *)D_004BF548,
    (char *)D_004BF538,
    (char *)D_004D8A80,
    (char *)D_004BF528,
    (char *)D_004BF518,
    (char *)D_004D8A80,
    (char *)D_004BF508
};

static char *maptbl1700[14] = {
    (char *)D_004BF628,
    (char *)D_004BF618,
    (char *)D_004BF608,
    (char *)D_004BF5F8,
    (char *)D_004BF5E8,
    (char *)D_004BF5D8,
    (char *)D_004BF5C8,
    (char *)D_004BF5B8,
    (char *)D_004BF5A8,
    (char *)D_004BF598,
    (char *)D_004BF588,
    (char *)D_004BF578,
    (char *)D_004BF568,
    (char *)D_004BF558
};

static char *maptbl1800[22] = {
    (char *)D_004BF738,
    (char *)D_004BF728,
    (char *)D_004BF718,
    (char *)D_004BF708,
    (char *)D_004BF6F8,
    (char *)D_004BF6E8,
    (char *)D_004BF6D8,
    (char *)D_004BF6C8,
    (char *)D_004BF6B8,
    (char *)D_004BF6A8,
    (char *)D_004BF698,
    (char *)D_004BF688,
    (char *)D_004BF678,
    (char *)D_004BF668,
    (char *)D_004BF658,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004D8A80,
    (char *)D_004BF648,
    (char *)D_004BF638
};

MapNameGroup maptbl[20] = {
    {maptbl0000, 58},
    {maptbl0100, 32},
    {maptbl0200, 0},
    {maptbl0300, 1},
    {maptbl0400, 2},
    {maptbl0500, 8},
    {maptbl0600, 2},
    {maptbl0700, 48},
    {maptbl0800, 19},
    {maptbl0900, 19},
    {maptbl1000, 1},
    {maptbl1100, 9},
    {maptbl1200, 41},
    {maptbl1300, 24},
    {maptbl1400, 25},
    {maptbl1500, 12},
    {maptbl1600, 7},
    {maptbl1700, 14},
    {maptbl1800, 22},
    {0, 0}
};



/* The retail initialized camera table contains 33 zeroed records. */
/* Existing Java camera setters establish 33 records of 160 bytes.
 * Both zero-initialized objects are genuine retail .data OBJECTs, not BSS. */
CfCameraDefinition CfCameraDefine[33] __attribute__((section(".data"))) = {{0}};

/* GameCameraStateSave/Restore copies the complete 0x5f0-byte camera image.
 * The eight-byte word view follows its existing CameraImage declaration. */

CameraImage save_cam __attribute__((section(".data"))) = {{0}};
