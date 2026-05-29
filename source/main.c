#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define MAX_POINTS    5000

typedef struct {
    u8 version;          
    u8 attributes;       
    u8 system_version;   
    u8 region_lock;      
    u64 system_id;       
    u32 mii_id;          
    u8 mac_address[6];   
    u16 padding1;
    u16 mii_name[10];    
    
    u8 body_height;      
    u8 body_weight;      
    u8 face_color;       
    u8 face_shape;       
    u8 face_wrinkles;    
    u8 face_makeup;      
    u8 hair_type;        
    u8 hair_color;       
    u8 hair_flip;        
    
    u32 eye_details;     
    u32 eyebrow_details; 
    u32 nose_details;    
    u32 mouth_details;   
    
    u8 creator_name[10]; 
    u16 checksum;        
} __attribute__((packed)) MiiData3DS;

typedef struct {
    u16 x;
    u16 y;
    bool is_continuous; 
} DrawingPoint;

DrawingPoint drawing[MAX_POINTS];
int pointCount = 0;
bool is_drawing_stroke = false;

u16 calculate_mii_checksum(const u8* data, size_t size) {
    u16 crc = 0x0000;
    for (size_t i = 0; i < size; i++) {
        crc ^= (data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void generate_mii_from_drawing(MiiData3DS *mii) {
    memset(mii, 0, sizeof(MiiData3DS));
    mii->version = 0x03; 
    mii->mii_name[0] = 'D'; mii->mii_name[1] = 'r'; mii->mii_name[2] = 'a'; 
    mii->mii_name[3] = 'w'; mii->mii_name[4] = 'n';
    
    srand(osGetTime());
    mii->mii_id = (u32)rand(); 
    
    mii->body_height = 64; 
    mii->body_weight = 64; 
    mii->face_shape = 0;   
    
    if (pointCount > 15) {
        u16 min_x = SCREEN_WIDTH, max_x = 0;
        u16 min_y = SCREEN_HEIGHT, max_y = 0;
        for (int i = 0; i < pointCount; i++) {
            if (drawing[i].x < min_x) min_x = drawing[i].x;
            if (drawing[i].x > max_x) max_x = drawing[i].x;
            if (drawing[i].y < min_y) min_y = drawing[i].y;
            if (drawing[i].y > max_y) max_y = drawing[i].y;
        }
        u16 drawing_width = max_x - min_x;
        u16 drawing_height = max_y - min_y;
        if (drawing_width > drawing_height + 25) {
            mii->face_shape = 3; 
        } else if (drawing_height > drawing_width + 25) {
            mii->face_shape = 1; 
        }
    }
    
    mii->eye_details = (0 & 0x3F) | ((2 & 0x0F) << 20);   
    mii->mouth_details = (0 & 0x3F) | ((10 & 0x1F) << 14); 
    mii->checksum = calculate_mii_checksum((u8*)mii, 94);
}

bool export_mii_to_sd(MiiData3DS *mii) {
    FILE *file = fopen("sdmc:/custom_mii.charinfo", "wb");
    if (file == NULL) return false; 
    size_t written = fwrite(mii, 1, sizeof(MiiData3DS), file);
    fclose(file);
    return written == sizeof(MiiData3DS);
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    cfguInit(); 
    C3d_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2d_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2d_Prepare();

    PrintConsole topConsole;
    consoleInit(GFX_TOP, &topConsole);
    C3D_RenderTarget* bottomTarget = C2d_CreateMainTarget(GFX_BOTTOM, GFX_LEFT);

    printf("\x1b[10;5H=== Mii Maker Drawer ===");
    printf("\x1b[12;5HDraw on the bottom touchscreen.");
    printf("\x1b[14;5H[A]     - Export Mii to SD card");
    printf("\x1b[15;5H[START] - Clear Canvas");
    printf("\x1b[16;5H[SELECT]- Exit App safely");

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_SELECT) break;        
        if (kDown & KEY_START) {
            pointCount = 0; 
            is_drawing_stroke = false;
        }

        if (kDown & KEY_A) {
            MiiData3DS myMii;
            generate_mii_from_drawing(&myMii);
            if (export_mii_to_sd(&myMii)) {
                printf("\x1b[19;5HSuccess! Saved to SD root.     ");
            } else {
                printf("\x1b[19;5H[ERROR] SD write failed!      ");
            }
        }

        if (kHeld & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (pointCount < MAX_POINTS) {
                drawing[pointCount].x = touch.px;
                drawing[pointCount].y = touch.py;
                drawing[pointCount].is_continuous = is_drawing_stroke;
                pointCount++;
            }
            is_drawing_stroke = true; 
        } else {
            is_drawing_stroke = false; 
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2d_TargetClear(bottomTarget, C2D_Color32(245, 245, 245, 255));
        C2d_SceneBegin(bottomTarget);

        C2d_DrawCircleSolid(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 0, 65, C2D_Color32(255, 235, 215, 255));

        for (int i = 0; i < pointCount; i++) {
            if (drawing[i].is_continuous && i > 0) {
                C2d_DrawLine(
                    drawing[i-1].x, drawing[i-1].y, C2D_Color32(40, 40, 40, 255),
                    drawing[i].x, drawing[i].y, C2D_Color32(40, 40, 40, 255),
                    3.5f, 0
                );
            } else {
                C2d_DrawCircleSolid(drawing[i].x, drawing[i].y, 0, 1.75f, C2D_Color32(40, 40, 40, 255));
            }
        }
        C3D_FrameEnd(0);
    }

    C2d_Fini();
    C3d_Fini();
    cfguExit();
    gfxExit();
    return 0;
}
