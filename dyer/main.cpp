

/*
 * Sifteo SDK Example.
 */
#define MAX_CUBES CUBE_ALLOCATION
#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

#define DYER_TEST
// static const unsigned gNumCubes = 2;
// Random gRandom;


static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);
static AssetSlot ASlot = AssetSlot::allocate()
    .bootstrap(CloseScene1);

static AssetSlot BSlot = AssetSlot::allocate()
    .bootstrap(CloseScene2);

    
static AssetSlot CSlot = AssetSlot::allocate()
    .bootstrap(CloseScene3);

static Metadata M = Metadata()
    .title("Dyer")
    .package("com.leon.dyer", "1.0")
    .icon(Icon)
    .cubeRange(1, CUBE_ALLOCATION);


VideoBuffer vid[CUBE_ALLOCATION];
TiltShakeRecognizer motion[MAX_CUBES];
char mask[128][128];
char center_careful_mask[128/16][128/16];

RGB565 colors[MAX_CUBES][4];
RGB565 target_colors[MAX_CUBES];

enum State
{
    kWorking,
    kFailed,
    kSucceed
};

struct CubeFlags
{
    char is_vertical: 1;
    char touching : 1;
    enum State state : 4;
};

CubeFlags cube_flags[CUBE_ALLOCATION];


RGB565 color_pool[] = {
    RGB565::fromRGB(0x8bc34a), // Light Green
    RGB565::fromRGB(0x00bcd4), // Cyan
    RGB565::fromRGB(0xffeb3b), // Yellow
    RGB565::fromRGB(0xe0e0e0), // Grey
    RGB565::fromRGB(0x009688), // Teal
    RGB565::fromRGB(0xef5350), // Read 
};

enum Constants
{
    kPixelsEachPacket = 16,

    kTop = 0,
    kLeft = 1,
    kDown = 2,
    kRight = 3,
    kNumColorPool = sizeof(color_pool) / sizeof(RGB565)
};

BitArray<kNumColorPool> color_pool_used;

struct ColorRGB
{
    float r;
    float g;
    float b;

    ColorRGB()
    {
        r = 0;
        g = 0;
        b = 0;
    }

    ColorRGB(uint8_t color)
    {
        r = color;
        g = color;
        b = color;
    }

    ColorRGB(float color)
    {
        r = color;
        g = color;
        b = color;
    }

    ColorRGB(RGB565 color)
    {
        r = color.red();
        g = color.green();
        b = color.blue();
    }

    static ColorRGB FromRGB565(RGB565 color)
    {
        ColorRGB rgb;
        rgb.r = color.red();
        rgb.g = color.green();
        rgb.b = color.blue();

        return rgb;
    }

    operator RGB565()
    {
        return ToRGB565();
    }

    RGB565 ToRGB565()
    {
        return RGB565::fromRGB((uint8_t) r, (uint8_t)g, (uint8_t)b);
    }

    bool ApproxEqual(ColorRGB rhs)
    {
        float dr = r - rhs.r;
        float dg = g - rhs.g;
        float db = b - rhs.b;

        return (dr * dr + dg * dg + db * db) < 20;
    }


    bool ExistsOneLessThan(ColorRGB rhs)
    {
        return r < rhs.r || g < rhs.g || b < rhs.b;
    }

    

};

#define BasicOp2(x) \
ColorRGB operator x(const ColorRGB& lhs, const ColorRGB& rhs) \
{\
    ColorRGB res;\
    res.r = rhs.r x lhs.r;\
    res.g = rhs.g x lhs.g;\
    res.b = rhs.b x lhs.b;\
    return res;\
}\
void operator x##=(ColorRGB& lhs, const ColorRGB& rhs) \
{\
    ColorRGB res;\
    lhs.r x##= rhs.r;\
    lhs.g x##= lhs.g;\
    lhs.b x##= lhs.b;\
}

BasicOp2(+)
BasicOp2(-)
BasicOp2(*)
BasicOp2(/)

#undef BasicOp2


class Dyer
{
public:
    RGB565 pixels[kPixelsEachPacket];
    BitArray<MAX_CUBES> cube_allow_pour;
    Random rng;

    

    void Init()
    {
        cube_allow_pour.mark();
        memset((uint8_t*)cube_flags, 0, sizeof(cube_flags) / sizeof(uint8_t));

        InitMask();
        InitColors();

        InitEvents();
        FirstPaint();
    }

    void Loop()
    {
        while (1)
        {
            System::paint();
        }
    }

    void EnterState(unsigned id, State state_)
    {
        cube_flags[id].state = state_;
    }

    State GetState(unsigned id)
    {
        return cube_flags[id].state;
    }

    void DisplayLose(unsigned cube)
    {
        vid[cube].initMode(BG0_SPR_BG1);
        vid[cube].attach(cube);

        vid[cube].sprites[0].move(vec(0, 0));
        vid[cube].sprites[0].setHeight(128);
        vid[cube].sprites[0].setWidth(128);

        vid[cube].sprites[0].setImage(ImLose);

        System::paint();
    }
    
    void FirstPaint()
    {
        for(auto id : CubeSet::connected())
        {
            vid[id].initMode(SOLID_MODE);
            vid[id].colormap[0] = colors[id][0];
        }

        System::paint();

        for(auto id : CubeSet::connected())
        {
            vid[id].initMode(STAMP);
            vid[id].stamp.disableKey();

            auto &fb = vid[id].stamp.initFB<16,1>();
            for (unsigned i = 0; i < 16; i++)
                fb.plot(vec(i, 0U), i);
        }

        for (unsigned y = 0; y < LCD_height; y++)
            for (unsigned x = 0; x < LCD_width; x += kPixelsEachPacket) {
                if(!center_careful_mask[x/16][y/16])
                {
                    continue;
                }
                System::finish();
                for(auto id : CubeSet::connected())
                {
                    for(int i  = 0; i < kPixelsEachPacket; ++i)
                    {
                        if(mask[y][x + i] == 0)
                        {
                            pixels[i] = colors[id][0];
                        }
                        else{
                            pixels[i] = target_colors[id];
                        }
                    }

                    vid[id].stamp.setBox(vec(x,y), vec((int)kPixelsEachPacket,1));
                    vid[id].colormap.set(pixels);
                }

                System::paintUnlimited();
                //System::paint();
            }
    }


    void FirstPaint(unsigned id)
    {
        vid[id].initMode(SOLID_MODE);
        vid[id].colormap[0] = colors[id][0];

        System::paint();

        vid[id].initMode(STAMP);
        vid[id].stamp.disableKey();
        cube_flags[id].is_vertical = 0;

        auto &fb = vid[id].stamp.initFB<16,1>();
        for (unsigned i = 0; i < 16; i++)
            fb.plot(vec(i, 0U), i);

        for (unsigned y = 0; y < LCD_height; y++)
            for (unsigned x = 0; x < LCD_width; x += kPixelsEachPacket) {
                if(!center_careful_mask[x/16][y/16])
                {
                    continue;
                }
                System::finish();
                    for(int i  = 0; i < kPixelsEachPacket; ++i)
                    {
                        if(mask[y][x + i] == 0)
                        {
                            pixels[i] = colors[id][0];
                        }
                        else{
                            pixels[i] = target_colors[id];
                        }
                    }

                    vid[id].stamp.setBox(vec(x,y), vec((int)kPixelsEachPacket,1));
                    vid[id].colormap.set(pixels);

                System::paintUnlimited();
                //System::paint();
            }
    }

    template<int y_dir, int y_start, int y_end, int x_dir, int x_start, int x_end, int vec_x, int vec_y>
    void RefreshPaintT(unsigned id, RGB565 foreground, RGB565 background)
    {
        for (unsigned y = y_start; y < y_end; y += y_dir)
            for (unsigned x = x_start; x < x_end; x += x_dir * kPixelsEachPacket) {
                for(int i  = 0; i < kPixelsEachPacket; ++i)
                {
                    if(mask[y][x + i] == 0)
                    {
                        pixels[i] = foreground;
                    }
                    else{
                        pixels[i] = background;
                    }
                }

                System::finish();
                vid[id].stamp.setBox(vec(x,y), vec(vec_x,vec_y));
                vid[id].colormap.set(pixels);
                System::paintUnlimited();
            }
    }

    void RefreshPaintTop(unsigned id, RGB565 foreground, RGB565 background)
    {
        for (unsigned y = 0; y < LCD_height; y++)
            for (unsigned x = 0; x < LCD_width; x += kPixelsEachPacket) {
                for(int i  = 0; i < kPixelsEachPacket; ++i)
                {
                    if(mask[y][x + i] == 0)
                    {
                        pixels[i] = foreground;
                    }
                    else{
                        pixels[i] = background;
                    }
                }

                System::finish();
                vid[id].stamp.setBox(vec(x,y), vec((int)kPixelsEachPacket, 1));
                vid[id].colormap.set(pixels);
                System::paintUnlimited();
            }
    }

    void RefreshPaintDonw(unsigned id, RGB565 foreground, RGB565 background)
    {
        for (int y = LCD_height - 1; y >= 0; y--)
            for (unsigned x = 0; x < LCD_width; x += kPixelsEachPacket) {
                for(int i  = 0; i < kPixelsEachPacket; ++i)
                {
                    if(mask[y][x + i] == 0)
                    {
                        pixels[i] = foreground;
                    }
                    else{
                        pixels[i] = background;
                    }
                }

                System::finish();
                vid[id].stamp.setBox(vec(x,(unsigned)y), vec((int)kPixelsEachPacket, 1));
                vid[id].colormap.set(pixels);
                System::paintUnlimited();
            }
    }

    

    void RefreshPaintLeft(unsigned id, RGB565 foreground, RGB565 background)
    {
        LOG("Repaint from left");
        for (unsigned x = 0; x < LCD_width; x++)
            for (unsigned y = 0; y < LCD_height; y += kPixelsEachPacket) {
                for(int i  = 0; i < kPixelsEachPacket; ++i)
                {
                    if(mask[y + i][x] == 0)
                    {
                        pixels[i] = foreground;
                    }
                    else{
                        pixels[i] = background;
                    }
                }

                System::finish();
                vid[id].stamp.setBox(vec(x,y), vec(1, (int)kPixelsEachPacket));
                vid[id].colormap.set(pixels);
                System::paintUnlimited();
            }
    }

    void RefreshPaintRight(unsigned id, RGB565 foreground, RGB565 background)
    {
        for (int x = LCD_width - 1; x >= 0; x--)
            for (unsigned y = 0; y < LCD_height; y += kPixelsEachPacket) {
                for(int i  = 0; i < kPixelsEachPacket; ++i)
                {
                    if(mask[y + i][x] == 0)
                    {
                        pixels[i] = foreground;
                    }
                    else{
                        pixels[i] = background;
                    }
                }

                System::finish();
                vid[id].stamp.setBox(vec((unsigned)x,y), vec(1, (int)kPixelsEachPacket));
                vid[id].colormap.set(pixels);
                System::paintUnlimited();
            }
    }

    void DoPaint(unsigned id, const Sifteo::PinnedAssetImage* images, int n)
    {
        for(int i = 0; i < n; ++i)
            {
                vid[id].sprites[0].setImage(images[i]);

                System::paint();

                // kill some time
                for(int j = 0; j < 5; ++j)
                {
                    System::paint();
                }
            }
    }

    void PlayImageSequence(unsigned id)
    {
        unsigned cube = id;

        vid[cube].initMode(BG0_SPR_BG1);
        vid[cube].attach(cube);

        vid[cube].sprites[0].move(vec(0, 0));
        vid[cube].sprites[0].setHeight(128);
        vid[cube].sprites[0].setWidth(128);

        

        DoPaint(id, ImageClose1, 16);
        DoPaint(id, ImageClose2, 16);
        DoPaint(id, ImageClose3, 16);
        DoPaint(id, ImageClose4, 4);
        
        // ResetVid(id);
    }



    void RefreshPaint(
        unsigned id, 
        RGB565 foreground, // 当前颜色 
        RGB565 background, // 目标颜色
        int direction)
    {
        if(cube_flags[id].is_vertical)
        {
            if(direction == kTop || direction == kDown)
            {
                vid[id].stamp.disableKey();

                auto &fb = vid[id].stamp.initFB<16,1>();
                for (unsigned i = 0; i < 16; i++)
                    fb.plot(vec(i, 0U), i);

                cube_flags[id].is_vertical = 0;
            }
            
        }
        else{
            if(direction == kLeft || direction == kRight)
            {
                vid[id].stamp.disableKey();

                auto &fb = vid[id].stamp.initFB<2,16>();
                for (unsigned i = 0; i < 16; i++)
                {
                    fb.plot(vec(0U, i), i);
                    fb.plot(vec(1U, i), i);
                }
                    

                cube_flags[id].is_vertical = 1;
            }
        }
        switch (direction)
        {
        case kLeft:
            RefreshPaintLeft(id, foreground, background);
            break;
        case kRight:
            RefreshPaintRight(id, foreground, background);
            break;
        case kTop:
            RefreshPaintTop(id, foreground, background);
            break;
        case kDown:
            RefreshPaintDonw(id, foreground, background);
            break;
        default:
            break;
        }

        ColorRGB target(target_colors[id]);
        ColorRGB cur(foreground);

        if(cur.ExistsOneLessThan(target))
        {
            EnterState(id, kFailed);
            DisplayLose(id);
        }
        else if(cur.ApproxEqual(target_colors[id]))
        {
            EnterState(id, kSucceed);
            PlayImageSequence(id);

        }
    }

    

    void ResetVid(unsigned id)
    {
        vid[id].initMode(STAMP);
        vid[id].stamp.disableKey();
        vid[id].attach(id);

        ResetFramebuffer(id);
        
    }

    void ResetFramebuffer(unsigned id)
    {
        if(cube_flags[id].is_vertical == 1)
        {
            auto &fb = vid[id].stamp.initFB<2,16>();
                for (unsigned i = 0; i < 16; i++)
                {
                    fb.plot(vec(0U, i), i);
                    fb.plot(vec(1U, i), i);
                }
        }
        else{
            auto &fb = vid[id].stamp.initFB<16,1>();
                for (unsigned i = 0; i < 16; i++)
                    fb.plot(vec(i, 0U), i);
        }
    }

    void OnConnect(unsigned id)
    {
        LOG("Cube %d connected\n", id);

        vid[id].initMode(STAMP);
        vid[id].stamp.disableKey();
        vid[id].attach(id);

        motion[id].attach(id);
        GetTargetColor(id);


        auto &fb = vid[id].stamp.initFB<16,1>();
        for (unsigned i = 0; i < 16; i++)
            fb.plot(vec(i, 0U), i);
        
    }

    void OnDisconnect(unsigned id)
    {
        LOG("Cube %d DISconnected\n", id);
    }

    RGB565 MixColor(RGB565 from1, RGB565 to2)
    {
        ColorRGB a(from1);
        ColorRGB b(to2);


        return ((b / 255.0f) * (a / 255.0f)) * 255.0f;
    }

private:
#ifdef DYER_TEST
    char test_steps[CUBE_ALLOCATION] = { 0 };
#endif
    
    void OnTouch(unsigned id)
    {
        CubeID cube(id);
        if(!cube.isTouching())
        {
            cube_flags[id].touching = 0;
            return;
        }
        if(cube_flags[id].touching == 0)
        {
            cube_flags[id].touching = 1;
            OnTouchDown(id);
        }

    }

    void OnTouchDown(unsigned id)
    {

    if(GetState(id) == kFailed)
    {
        EnterState(id, kWorking);
        LOG("State going from Failed to working\n");

        FirstPaint(id);

    }
    else if(GetState(id) == kSucceed)
    {
        EnterState(id, kWorking);
        FirstPaint(id);
        LOG("State going from Succeed to working\n");
    }

    #ifdef DYER_TEST
    test_steps[id]++;
    switch (test_steps[id])
    {
    case 1:
        EnterState(id, kFailed);
        DisplayLose(id);
        break;
    case 4:
        EnterState(id, kSucceed);
        PlayImageSequence(id);
        break;
    
    default:
        break;
    }
    #endif
    }

    void OnAcclChange(unsigned id)
    {
        unsigned changeFlags = motion[id].update();
        if (!changeFlags) {
            return;
        }
        int target = 0;
        auto& tilt = motion[id].tilt;

        
        // 如果方块复位
        if(tilt.x == 0 && tilt.y == 0)
        {
            cube_allow_pour.mark(id);
            return;
        }

        

        // 在倾倒之前方块没有复位
        if(!cube_allow_pour.test(id))
        {
            return;
        }

        
        LOG("Check direction\n");
        // 检查往哪里倾倒
        if(motion[id].tilt.x > 0)
        {
            target = kRight;
        }
        else if(motion[id].tilt.x < 0)
        {
            target = kLeft;
        }
        else if(motion[id].tilt.y > 0)
        {
            target = kDown;
        }
        else if(motion[id].tilt.y < 0)
        {
            target = kTop;
        }

        // 查找倾倒方向是否有邻居
        Neighborhood neigborhood(id);
        auto neigbor = neigborhood.neighborAt((Side)target);

        if(!neigbor.isCube())
        {
            return;
        }

        
        // 检查这个邻居是否和我们的方向是一样的
        unsigned nid = neigbor.cube();
        motion[nid].update();
        LOG("t.x %d, t.y %d\n", tilt.x, tilt.y);
        LOG("t.x %d, t.y %d\n", motion[nid].tilt.x, motion[nid].tilt.y);
        LOG("Cube %d Check Neigbor...", id);

        //if(motion[nid].tilt != tilt)
        //{
           // LOG("Failed\n");
            //return;
        //}
        LOG("id = %d\n", nid);

        // 标注方块处在非复位状态, 主要为了防止反复倾倒
        cube_allow_pour.clear(id);

        // 倾倒方块在 被倾倒的方块 的那个方向上
        int side = Neighborhood(nid).sideOf(id);
        auto finalColor = MixColor(colors[id][0], colors[nid][0]);
        RefreshPaint(nid, finalColor, target_colors[nid], side);

        LOG("Cube %d pours to cube %d\n", id, nid);
    }

    void InitMask()
    {
        memset32((uint32_t*)mask, 0, sizeof(mask) / sizeof(uint32_t));
        memset32((uint32_t*)center_careful_mask, 0, sizeof(center_careful_mask) / sizeof(uint32_t));

        for(int i = 0; i < LCD_width; ++i)
        {
            for(int j = 0; j < LCD_height; ++j)
            {
                int x = i - LCD_width / 2;
                int y = j - LCD_height / 2;
                if(x*x + y * y <= 16 * 16)
                {
                    mask[i][j] = 1;

                    center_careful_mask[i / 16][j / 16] = 1;
                }
                
            }
        }
    }

    void InitEvents()
    {
        Events::cubeConnect.set(&Dyer::OnConnect, this);
        Events::cubeDisconnect.set(&Dyer::OnDisconnect, this);
        Events::cubeAccelChange.set(&Dyer::OnAcclChange, this);

        #ifdef DYER_TEST

        Events::cubeTouch.set(&Dyer::OnTouch, this);

        #endif

        for (CubeID cube : CubeSet::connected())
            OnConnect(cube);
    }

    void AssignColor(unsigned cube)
    {
        int color = rng.randint(0, kNumColorPool - 1);
        if(color_pool_used.test(color))
        {
            color = rng.randint(0, kNumColorPool - 1);
            for(int k = 0; k < 10 && color_pool_used.test(color); ++k)
            {   
                color = rng.randint(0, kNumColorPool - 1);
            }
        }
        else{
            color_pool_used.mark(color);
        }
        colors[cube][0] = color_pool[color];
    }


    void InitColors()
    {
        BitArray<CUBE_ALLOCATION> color_assigned;
        color_assigned.clear();

        for (CubeID cube : CubeSet::connected())
        {
            AssignColor(cube);
            color_assigned.mark(cube);
        }

        for(int i = 0; i < CUBE_ALLOCATION; ++i)
        {
            if(!color_assigned.test(i))
            {
                AssignColor(i);
            }
        }
    }

    void GetTargetColor(CubeID target)
    {
        Array<CubeID, CUBE_ALLOCATION> alive;
        for (CubeID cube : CubeSet::connected())
        {
            if(cube == target)
            {
                continue;
            }

            alive.append(cube);
        }

        while (alive.count() > 1)
        {
            int i = rng.randint(0, (int)alive.count() - 1);
            alive.erase(i);
        }
        
        ColorRGB target_color = colors[target][0];

        for(int i = 0; i < alive.count(); ++i)
        {
            target_color = MixColor(colors[alive[i]][0], target_color);
        }

        target_colors[target] = target_color;
    }
    
};


void main()
{
    Dyer dyer;
    dyer.Init();
    dyer.Loop();
}