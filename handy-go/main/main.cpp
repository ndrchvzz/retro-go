extern "C" {
#include <rg_system.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
}

#include <handy.h>

#define APP_ID 50

#define AUDIO_SAMPLE_RATE   (HANDY_AUDIO_SAMPLE_FREQ)
#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / 40)

static short audioBuffer[AUDIO_BUFFER_LENGTH * 2];

static rg_video_frame_t update1;
static rg_video_frame_t update2;
static rg_video_frame_t *currentUpdate = &update1;
static rg_video_frame_t *previousUpdate = &update2;

static CSystem *lynx = NULL;

static int dpad_mapped_up;
static int dpad_mapped_down;
static int dpad_mapped_left;
static int dpad_mapped_right;
// static bool netplay = false;
// --- MAIN

static void set_rotation()
{
    display_rotation_t rotation = rg_display_get_rotation();

    if (rotation == RG_DISPLAY_ROTATION_AUTO)
    {
        rotation = RG_DISPLAY_ROTATION_OFF;

        switch (lynx->mCart->CRC32())
        {
            case 0x97501709: // Centipede
            case 0x0271B6E9: // Lexis
            case 0x006FD398: // NFL Football
            case 0xBCD10C3A: // Raiden
                rotation = RG_DISPLAY_ROTATION_LEFT;
                break;
            case 0x7F0EC7AD: // Gauntlet
            case 0xAC564BAA: // Gauntlet - The Third Encounter
            case 0xA53649F1: // Klax
                rotation = RG_DISPLAY_ROTATION_RIGHT;
                break;
            default:
                if (lynx->mCart->CartGetRotate() == CART_ROTATE_LEFT)
                    rotation = RG_DISPLAY_ROTATION_LEFT;
                if (lynx->mCart->CartGetRotate() == CART_ROTATE_RIGHT)
                    rotation = RG_DISPLAY_ROTATION_RIGHT;
        }
    }

    switch(rotation)
    {
        case RG_DISPLAY_ROTATION_LEFT:
            update1.width = update2.width = HANDY_SCREEN_HEIGHT;
            update1.height = update2.height = HANDY_SCREEN_WIDTH;
            lynx->mMikie->SetRotation(MIKIE_ROTATE_L);
            dpad_mapped_up    = BUTTON_RIGHT;
            dpad_mapped_down  = BUTTON_LEFT;
            dpad_mapped_left  = BUTTON_UP;
            dpad_mapped_right = BUTTON_DOWN;
            break;
        case RG_DISPLAY_ROTATION_RIGHT:
            update1.width = update2.width = HANDY_SCREEN_HEIGHT;
            update1.height = update2.height = HANDY_SCREEN_WIDTH;
            lynx->mMikie->SetRotation(MIKIE_ROTATE_R);
            dpad_mapped_up    = BUTTON_LEFT;
            dpad_mapped_down  = BUTTON_RIGHT;
            dpad_mapped_left  = BUTTON_DOWN;
            dpad_mapped_right = BUTTON_UP;
            break;
        default:
            update1.width = update2.width = HANDY_SCREEN_WIDTH;
            update1.height = update2.height = HANDY_SCREEN_HEIGHT;
            lynx->mMikie->SetRotation(MIKIE_NO_ROTATE);
            dpad_mapped_up    = BUTTON_UP;
            dpad_mapped_down  = BUTTON_DOWN;
            dpad_mapped_left  = BUTTON_LEFT;
            dpad_mapped_right = BUTTON_RIGHT;
            break;
    }
}


static bool rotation_cb(dialog_choice_t *option, dialog_event_t event)
{
    int rotation = (int)rg_display_get_rotation();

    if (event == RG_DIALOG_PREV) {
        if (--rotation < 0) rotation = RG_DISPLAY_ROTATION_COUNT - 1;
        rg_display_set_rotation((display_rotation_t)rotation);
        set_rotation();
    }
    if (event == RG_DIALOG_NEXT) {
        if (++rotation > RG_DISPLAY_ROTATION_COUNT - 1) rotation = 0;
        rg_display_set_rotation((display_rotation_t)rotation);
        set_rotation();
    }

    strcpy(option->value, "Off  ");
    if (rotation == RG_DISPLAY_ROTATION_AUTO)  strcpy(option->value, "Auto ");
    if (rotation == RG_DISPLAY_ROTATION_LEFT)  strcpy(option->value, "Left ");
    if (rotation == RG_DISPLAY_ROTATION_RIGHT) strcpy(option->value, "Right");

    return event == RG_DIALOG_ENTER;
}

static bool save_state(char *pathName)
{
    bool ret = false;
    FILE *fp;

    if ((fp = fopen(pathName, "wb")))
    {
        ret = lynx->ContextSave(fp);
        fclose(fp);

        char *filename = rg_emu_get_path(EMU_PATH_SCREENSHOT, 0);
        if (filename)
        {
            rg_display_save_frame(filename, currentUpdate, 1);
            rg_free(filename);
        }
    }

    return ret;
}


static bool load_state(char *pathName)
{
    bool ret = false;
    FILE *fp;

    if ((fp = fopen(pathName, "rb")))
    {
        ret = lynx->ContextLoad(fp);
        fclose(fp);
    }

    if (!ret) lynx->Reset();

    return ret;
}


extern "C" void app_main(void)
{
    heap_caps_malloc_extmem_enable(32 * 1024);
    rg_system_init(APP_ID, AUDIO_SAMPLE_RATE);
    rg_emu_init(&load_state, &save_state, NULL);

    update1.width = update2.width = HANDY_SCREEN_WIDTH;
    update1.height = update2.height = HANDY_SCREEN_WIDTH;
    update1.stride = update2.stride = HANDY_SCREEN_WIDTH * 2;
    update1.pixel_size = update2.pixel_size = 2;
    update1.pixel_clear = update2.pixel_clear = -1;

    update1.buffer = (void*)rg_alloc(update1.stride * update1.height, MEM_FAST);
    update2.buffer = (void*)rg_alloc(update2.stride * update2.height, MEM_FAST);

    rg_app_desc_t *app = rg_system_get_app();

    // Init emulator
    lynx = new CSystem(app->romPath, MIKIE_PIXEL_FORMAT_16BPP_565_BE, AUDIO_SAMPLE_RATE);

    if (lynx->mFileType == HANDY_FILETYPE_ILLEGAL)
    {
        RG_PANIC("ROM loading failed!");
    }

    gPrimaryFrameBuffer = (UBYTE*)currentUpdate->buffer;
    gAudioBuffer = (SWORD*)&audioBuffer;
    gAudioEnabled = 1;

    if (app->startAction == EMU_START_ACTION_RESUME)
    {
        rg_emu_load_state(0);
    }

    set_rotation();

    float sampleTime = AUDIO_SAMPLE_RATE / 1000000.f;
    long skipFrames = 0;
    bool fullFrame = 0;

    // Start emulation
    while (1)
    {
        gamepad_state_t joystick = rg_input_read_gamepad();

        if (joystick.values[GAMEPAD_KEY_MENU]) {
            rg_gui_game_menu();
        }
        else if (joystick.values[GAMEPAD_KEY_VOLUME]) {
            dialog_choice_t options[] = {
                {100, "Rotation", "Auto", 1, &rotation_cb},
                RG_DIALOG_CHOICE_LAST
            };
            rg_gui_game_settings_menu(options);
        }

        int64_t startTime = get_elapsed_time();
        bool drawFrame = !skipFrames;

        ULONG buttons = 0;

    	if (joystick.values[GAMEPAD_KEY_UP])     buttons |= dpad_mapped_up;
    	if (joystick.values[GAMEPAD_KEY_DOWN])   buttons |= dpad_mapped_down;
    	if (joystick.values[GAMEPAD_KEY_LEFT])   buttons |= dpad_mapped_left;
    	if (joystick.values[GAMEPAD_KEY_RIGHT])  buttons |= dpad_mapped_right;
    	if (joystick.values[GAMEPAD_KEY_A])      buttons |= BUTTON_A;
    	if (joystick.values[GAMEPAD_KEY_B])      buttons |= BUTTON_B;
    	if (joystick.values[GAMEPAD_KEY_START])  buttons |= BUTTON_OPT2; // BUTTON_PAUSE
    	if (joystick.values[GAMEPAD_KEY_SELECT]) buttons |= BUTTON_OPT1;

        lynx->SetButtonData(buttons);

        lynx->UpdateFrame(drawFrame);

        if (drawFrame)
        {
            fullFrame = rg_display_queue_update(currentUpdate, previousUpdate) == SCREEN_UPDATE_FULL;
            previousUpdate = currentUpdate;
            currentUpdate = (currentUpdate == &update1) ? &update2 : &update1;
            gPrimaryFrameBuffer = (UBYTE*)currentUpdate->buffer;
        }

        long elapsed = get_elapsed_time_since(startTime);

        // See if we need to skip a frame to keep up
        if (skipFrames == 0)
        {
            if (app->speedupEnabled)
                skipFrames += app->speedupEnabled * 2.5;
            // The Lynx uses a variable framerate so we use the count of generated audio samples as reference instead
            else if (elapsed > ((gAudioBufferPointer/2) * sampleTime))
                skipFrames += 1;
            else if (drawFrame && fullFrame) // This could be avoided when scaling != full
                skipFrames += 1;
        }
        else if (skipFrames > 0)
        {
            skipFrames--;
        }

        rg_system_tick(!drawFrame, fullFrame, elapsed);

        if (!app->speedupEnabled)
        {
            rg_audio_submit(gAudioBuffer, gAudioBufferPointer >> 1);
            gAudioBufferPointer = 0;
        }
    }
}
