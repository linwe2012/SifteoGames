/*
 * Sifteo SDK Example.
 */

#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

#define MAX_CUBES CUBE_ALLOCATION

#define MAX_PHRASES 2
#define MAX_IMAGE2x2 2

static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);

static AssetSlot ASlot = AssetSlot::allocate()
    .bootstrap(WordAssets);

static Metadata M = Metadata()
    .title("WordGame")
    .package("com.leon.health.word", "1.0")
    .icon(Icon)
    .cubeRange(0, CUBE_ALLOCATION);

template<typename T>
void Swap(T& a, T& b)
{
    T c = a;
    a = b;
    b = c;
}

struct Topology
{
    short left;
    short right;
    short top;
    short down;
    short word_arrange;
    char valid;
    char rotate;
};



static VideoBuffer vid[MAX_CUBES];
static TiltShakeRecognizer motion[MAX_CUBES];
// static char Text[MAX_CUBES][4];
static char TextAssetId[MAX_CUBES][2];
static Topology top[MAX_CUBES];
static Topology expected[MAX_CUBES];
static Array<int, CUBE_ALLOCATION> alive_cubes;
static char* error = NULL;

enum WordMode
{
    WM_None,
    WM_Phrases4,
    WM_Image2x2,
    WM_Image3x3
};



WordMode word_mode = WM_Phrases4;
int assets_id = 0;

class Word {
public:
    static Int2 lerp(Int2 a, Int2 b, double c){
        Int2 res;
        res.x = a.x * c + b.x * (1-c);
        res.y = a.y * c + b.y * (1-c);
        return res;
    }

    void install()
    {
        for(int i = 0;  i < MAX_CUBES; ++i)
        {
            top[i].left = -1;
            top[i].right = -1;
            top[i].word_arrange = -1;
            top[i].valid = 0;
        }


        Events::neighborAdd.set(&Word::onNeighborAdd, this);
        Events::neighborRemove.set(&Word::onNeighborRemove, this);
        Events::cubeAccelChange.set(&Word::onAccelChange, this);
        // Events::cubeTouch.set(&Word::onTouch, this);
        // Events::cubeBatteryLevelChange.set(&Word::onBatteryChange, this);
        Events::cubeConnect.set(&Word::onConnect, this);
        Events::cubeDisconnect.set(&Word::onDisconnect, this);

        // Handle already-connected cubes
        for (CubeID cube : CubeSet::connected())
            onConnect(cube);
    }

    static void UpdateArrange(int id)
    {
        int word_arrange = top[id].word_arrange;
        auto v1 = vec(0, 0);
        auto v2 = vec(64, 0);

        if(word_arrange)
        {
            Swap(v1, v2);
        }

        

        if(word_mode == WM_Phrases4)
        {
            int txt1 = (int)TextAssetId[id][0];
            int txt2 = (int)TextAssetId[id][1];

            LOG("Cube %d is diaplay assets: %d, %d from phrases\n", id, txt1, txt2);

            vid[id].sprites[0].setWidth(64);
            vid[id].sprites[0].setHeight(128);
            vid[id].sprites[0].setImage(Words4[txt1]);
            

            vid[id].sprites[1].setWidth(64);
            vid[id].sprites[1].setHeight(128);
            vid[id].sprites[1].setImage(Words4[txt2]);

            
            for(int i =0; i < 10; ++i)
            {
                double f = (i+1) / 10.0;
                vid[id].sprites[0].move(lerp(v1, v2, f));
                vid[id].sprites[1].move(lerp(v2, v1, f));
                System::paint();
            }

            //vid[id].sprites[0].move(v1);
            //vid[id].sprites[0].move(v2);
            //vid[id].sprites[0].move(v1);
            //vid[id].sprites[1].move(v2);
        }
    }
    

    void InitBackground(int refresh)
    {
        if(!refresh && (last_word_mode == word_mode))
        {
            return;
        }

        last_word_mode = word_mode;
        if(word_mode == WM_Phrases4){
            for(int i = 0; i < alive_cubes.count(); ++i)
            {
                int id = alive_cubes[i];
                vid[id].bg0.image(vec(0, 0), WordBG);
                vid[id].bg0.image(vec(9, 0), WordBG);
            }
        }
        else {
            const PinnedAssetImage* assets = word_mode == WM_Image2x2 ? Image2x2 : Image3x3;

            for(int i =0; i< alive_cubes.count(); ++i)
            {
                int id = alive_cubes[i];
                if(expected[id].valid)
                {
                    int text = TextAssetId[id][0];

                    // vid[id].bg0.image(vec(0, 0), assets[text]);
                    vid[id].sprites[1].setHeight(0);
                    vid[id].sprites[0].setHeight(128);
                    vid[id].sprites[0].setWidth(128);
                    vid[id].sprites[0].move(vec(0, 0));
                    vid[id].sprites[0].setImage(assets[text]);
                }
            }
        }
    }
private:

    void onDisconnect(unsigned id)
    {
        LOG("Cube %d DISconnected\n", id);
        top[id].valid = 0;
        for(int i = 0; i < alive_cubes.count(); ++i)
        {
            if(alive_cubes[i] == id)
            {
                alive_cubes.erase(i);
                break;
            }
        }
    }

    void onConnect(unsigned id)
    {
        if(top[id].valid) return;


        CubeID cube(id);
        uint64_t hwid = cube.hwID();
        LOG("Cube %d connected\n", id);

        vid[id].initMode(BG0_SPR_BG1);
        vid[id].attach(id);

        
        motion[id].attach(id);

        // Draw initial state for all sensors
        onAccelChange(cube);

        top[id].left = -1;
        top[id].right = -1;
        top[id].valid = 1;
        alive_cubes.push_back(id);
    }


    void onAccelChange(unsigned id)
    {
        CubeID cube(id);
        if(!expected[id].valid || word_mode != WM_Phrases4)
        {
            return;
        }

        // LOG("Cube %d Acccleatre: %d\n", id, accel.x);
        unsigned changeFlags = motion[id].update();
        if (changeFlags) {
            auto last = top[id].word_arrange;
            int word_arrange = 0;
            auto tilt = motion[id].tilt;
            if(tilt.x > 0)
            {
                word_arrange = 0;
            }
            else if(tilt.x < 0) {
                word_arrange = 1;
            }
            else {
                return;
            }
            if(last == word_arrange) return;
            top[id].word_arrange = word_arrange;

            UpdateArrange(id);

        }
    }

    WordMode last_word_mode = WM_None;
    

    void SetNeigbor(int ID, int side, int neigbor)
    {
        if(side == 1)
        {
            top[ID].left = neigbor;
        }
        else if(side == 0)
        {
            top[ID].top = neigbor;
        }
        else if(side == 3)
        {
            top[ID].right = neigbor;
        }
        else if(side == 2)
        {
            top[ID].down = neigbor;
        }
    }

    void onNeighborRemove(unsigned firstID, unsigned firstSide, unsigned secondID, unsigned secondSide)
    {
        LOG("Neighbor Remove: %02x:%d - %02x:%d\n", firstID, firstSide, secondID, secondSide);
        SetNeigbor(firstID, firstSide, -1);
        SetNeigbor(secondID, secondSide, -1);
    }

    void onNeighborAdd(unsigned firstID, unsigned firstSide, unsigned secondID, unsigned secondSide)
    {
        LOG("Neighbor Added: %02x:%d - %02x:%d\n", firstID, firstSide, secondID, secondSide);
        SetNeigbor(firstID, firstSide, secondID);
        SetNeigbor(secondID, secondSide, firstID);
    }
};



struct Manager
{
    Array<int, MAX_CUBES> shuffle;
    short cur_cubes;
    char is_3d;
    int mw;
    int mh;

    void Init()
    {
        for(int i = 0; i < MAX_CUBES; ++i)
        {
            expected[i].valid = 0;
        }
        shuffle.setCount(MAX_CUBES);
        cur_cubes = 0;
        is_3d = 0;
    }

    int TestEuqalPhase()
    {
        int failed = 0;
        for(int i = 0 ; i < MAX_CUBES; ++i)
        {
            // 这个方块是否要检查
            if(expected[i].valid == 1)
            {
                if( top[i].valid   // 这一步判设备是否在线
                    && (expected[i].left == -1  // 要么方块左边的积木应该是空的, 那么任何方块(比如多余的方块) 也判定是对的
                        || (expected[i].left == top[i].left))
                    && (expected[i].right == -1  // 要么方块右边的积木应该是空的, 那么任何方块(比如多余的方块) 也判定是对的
                        || (expected[i].right == top[i].right))
                    && expected[i].word_arrange == top[i].word_arrange
                    ){
                        
                    }
                else {
                    failed = 1;
                    break;
                }
            }
        }
        return !failed;
    }

    int TestEqualImage()
    {
        int failed = 0;
        for(int i = 0 ; i < MAX_CUBES; ++i)
        {
            // 这个方块是否要检查
            if(expected[i].valid == 1)
            {
                if( top[i].valid   // 这一步判设备是否在线
                    && (expected[i].left == -1  // 要么方块左边的积木应该是空的, 那么任何方块(比如多余的方块) 也判定是对的
                        || (expected[i].left == top[i].left))
                    && (expected[i].right == -1  // 要么方块右边的积木应该是空的, 那么任何方块(比如多余的方块) 也判定是对的
                        || (expected[i].right == top[i].right))
                    && (expected[i].top == -1  // 要么方块右边的积木应该是空的, 那么任何方块(比如多余的方块) 也判定是对的
                        || (expected[i].top == top[i].top))
                    &&  (expected[i].down == -1  // 要么方块右边的积木应该是空的, 那么任何方块(比如多余的方块) 也判定是对的
                        || (expected[i].down == top[i].down))
                    ){
                        
                    }
                else {
                    LOG("Checking for cube %d failed, [%d,%d,%d,%d] [%d,%d,%d,%d]\n", i,
                        expected[i].top, expected[i].left, expected[i].down, expected[i].right,
                        top[i].top, top[i].left, top[i].down, top[i].right);

                    failed = 1;
                    break;
                }
            }
        }
        return !failed;
    }

    int TestEqual()
    {
        if(word_mode == WM_Phrases4)
        {
            return TestEuqalPhase();
        }
        else {
            return TestEqualImage();
        }
    }

    Random rand;

    int within(int i, int low, int hi)
    {
        return i >= low && i < hi;
    }

    int ResetNxN(int asset_id, int w, int h)
    {
        int all = w * h;
        if(all > alive_cubes.count())
        {
            LOG("Not enough cubes, at least 4 required\n");
            return -1;
        }
        mw = w;
        mh = h;
        InitializeExpectation(w * h);

        int cnt = 0;
        for(int i = 0;  i < h; ++i)
        {
            for(int j = 0;  j < w; ++j)
            {
                // short rotate = rand.randrange(0, 4);
                const short rotate = 0; //TODO: 使用 BG2 模式绘制旋转
                short old_sides[] = {
                    (i-1)*w + j, // top
                    i * w + j - 1, // left
                    (i+1)*w + j, // down
                    (i)*w+j+1 // right   
                };

                if(i - 1 < 0) old_sides[0] = -1;
                if(j - 1 < 0) old_sides[1] = -1;
                if(i+1 >= h) old_sides[2] = -1;
                if(j+1>=w) old_sides[3] = -1;
                short new_sides[4];

                for(short i = 0; i < 4; ++i)
                {
                    new_sides[(rotate + i) % 4] = old_sides[i];
                }

                
                
                int id = shuffle[i*w+j];
                struct Topology& t = expected[id];

                if(within(new_sides[0], 0, all)) t.top = shuffle[new_sides[0]];
                if(within(new_sides[1], 0, all)) t.left = shuffle[new_sides[1]];
                if(within(new_sides[2], 0, all)) t.down = shuffle[new_sides[2]];
                if(within(new_sides[3], 0, all)) t.right = shuffle[new_sides[3]];

                t.rotate = rotate;
                top[id].rotate = rotate;

                TextAssetId[id][0] = asset_id * all + i * w + j;
                t.valid = 1;
                ++cnt;
            }
        }
        return 0;
    }

    void InitializeExpectation(int mlen)
    {
        // 初始化
        for(int i = 0; i < MAX_CUBES; ++i)
        {
            expected[i].left = -1;
            expected[i].right = -1;
            expected[i].top = -1;
            expected[i].down = -1;
            expected[i].valid = 0;
            expected[i].word_arrange = 0;
        }

        // 读入所有要使用的设备
        for(int i = 0;  i < mlen; ++i)
        {
            shuffle[i] = alive_cubes[i];
        }

        // 打乱设备
        for(int i = 0; i < mlen; ++i)
        {
            int idx = rand.randrange(0, mlen);
            Swap(shuffle[i], shuffle[idx]);
        }
    }

    /*
    int Reset(const char* str)
    {
        LOG("Begin Reset: %s\n", str);
        int len = strnlen(str, MAX_CUBES * 2);
        int mlen = len / 2 + (len % 2);
        cur_cubes = mlen;

        if(mlen > alive_cubes.count())
        {
            LOG("Not enough cubes, at least %d required\n", mlen);
            return -1;
        }

        InitializeExpectation(mlen);

        for(int i = 0; i < mlen; ++i)
        {
            int id = shuffle[i];
            expected[id].valid = 1;

            // 确定每个方块左右两边应该是啥方块
            if(i == mlen - 1)
            {
                expected[id].right = -1;
            }
            else{
                expected[id].right = shuffle[i+1];
            }

            if(i == 0)
            {
                expected[id].left = -1;
            }
            else{
                expected[id].left = shuffle[i-1];
            }

            
            expected[id].word_arrange = rand.randrange(0, 2);
            
            if(expected[id].word_arrange == 0)
            {
                Text[id][0] = str[i*2];
                Text[id][1] = '\0';

                Text[id][2] = str[i*2+1] ? str[i*2+1] : ' ';
                Text[id][3] = '\0';
            }

            else if(expected[id].word_arrange == 1)
            {
                Text[id][0] = str[i*2+1] ? str[i*2+1] : ' ';
                Text[id][1] = '\0';

                Text[id][2] = str[i*2];
                Text[id][3] = '\0';
            }

            top[id].word_arrange = rand.randrange(0, 2);
            LOG("Word Chunk: '%s%s' is assigned to cube %d\n", Text[id]+0, Text[id]+2, id);
            // 刷新屏幕显示
            Word::UpdateArrange(id);
        }*/


    int Reset(int asset_id, int mlen)
    {
        cur_cubes = mlen;
        assets_id = asset_id;

        if(mlen > alive_cubes.count())
        {
            LOG("Not enough cubes, at least %d required\n", mlen);
            return -1;
        }

        InitializeExpectation(mlen);

        for(int i = 0; i < mlen; ++i)
        {
            int id = shuffle[i];
            expected[id].valid = 1;

            // 确定每个方块左右两边应该是啥方块
            if(i == mlen - 1)
            {
                expected[id].right = -1;
            }
            else{
                expected[id].right = shuffle[i+1];
            }

            if(i == 0)
            {
                expected[id].left = -1;
            }
            else{
                expected[id].left = shuffle[i-1];
            }

            
            expected[id].word_arrange = rand.randrange(0, 2);
            
            if(expected[id].word_arrange == 0)
            {
                TextAssetId[id][0] = asset_id * 4 + i * 2;
                TextAssetId[id][1] = asset_id * 4 + 1 + i * 2;
            }

            else if(expected[id].word_arrange == 1)
            {
                TextAssetId[id][0] = asset_id * 4 + 1 + i * 2;
                TextAssetId[id][1] = asset_id * 4 + i * 2;
            }

            top[id].word_arrange = rand.randrange(0, 2);
            // LOG("Word Chunk: '%s%s' is assigned to cube %d\n", Text[id]+0, Text[id]+2, id);
            // 刷新屏幕显示
            Word::UpdateArrange(id);
        }
        return 0;

    }
};

// 三个积木才能玩的
#define WORD_LIST_3(V)\
V(World)\
V(Hello wo)\
V(ZZPBGD)\
V(GXYTQL)

#define INERVEL 10

void main()
{
    AudioTracker::play(Music);

    static Word sensors;

    memset((uint8_t*)&top, 0, sizeof(top));

    sensors.install();

    Manager man;
    man.Init();

    int cnt = 1;

    const char* words3[] = {
        #define MAKW_WORD(x) #x,
        WORD_LIST_3(MAKW_WORD)
        #undef MAKW_WORD
    };
    word_mode = WM_Phrases4;
    man.Reset(0, 2);
    sensors.InitBackground(0);

    int frames = 0;
    // We're entirely event-driven. Everything is
    // updated by SensorListener's event callbacks.
    int faild = 0;
    int loop_cnt = 0;

    while (1)
    {
        if(man.TestEqual() || (faild && loop_cnt == INERVEL))
        {
            if(cnt < MAX_PHRASES)
            {
                LOG("Refreshing Words\n");
                word_mode = WM_Phrases4;
                faild = man.Reset(cnt, 2);
                sensors.InitBackground(0);
            }
            else if(cnt < MAX_PHRASES + MAX_IMAGE2x2)
            {
                LOG("Refreshing Images\n");
                word_mode = WM_Image2x2;
                faild = man.ResetNxN(cnt - MAX_PHRASES, 2, 2);
                sensors.InitBackground(1);
            }

            ++cnt;
            loop_cnt = (loop_cnt+1) % (INERVEL+1);
        }
        
        System::paint();
        ++frames;
    }
    //return 0;
}
