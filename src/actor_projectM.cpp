#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include <libvisual/libvisual.h>

#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>

#include "ConfigFile.h"

std::string read_config();

static int gx = 48;
static int gy = 32;
static int wvw = 512;
static int wvh = 512;
static int fps = 60;
static double preset_duration = 30.0;
static double hardcut_duration = 10.0;
static double hardcut_sensitivity = 10.0;
static double transition_duration = 3.0;
static bool fullscreen = false;
static std::string preset_path;
static std::string texture_path;

/* Private context-sensitive data goes here, */
typedef struct
{
    projectm_handle PM;
    projectm_playlist_handle PL;
} ProjectmPrivate;

extern "C" int lv_projectm_init(VisPluginData* plugin);
extern "C" int lv_projectm_cleanup(VisPluginData* plugin);
extern "C" int lv_projectm_requisition(VisPluginData* plugin, int* width, int* height);
extern "C" int lv_projectm_dimension(VisPluginData* plugin, VisVideo* video, int width, int height);
extern "C" int lv_projectm_events(VisPluginData* plugin, VisEventQueue* events);
extern "C" VisPalette* lv_projectm_palette(VisPluginData* plugin);
extern "C" int lv_projectm_render(VisPluginData* plugin, VisVideo* video, VisAudio* audio);

VISUAL_PLUGIN_API_VERSION_VALIDATOR

/* Main plugin stuff */
/* The get_plugin_info function provides the libvisual plugin registry, and plugin loader
 * with the very basic plugin information */
extern "C" const VisPluginInfo* get_plugin_info(int* count)
{
    /* Initialize the plugin specific data structure
     * with pointers to the functions that represent
     * the plugin interface it's implementation, more info:
     * http://libvisual.sourceforge.net/newdocs/docs/html/struct__VisActorPlugin.html */
    static VisActorPlugin actor[1];
    static VisPluginInfo info[1];

    actor[0].requisition = lv_projectm_requisition;
    actor[0].palette = lv_projectm_palette;
    actor[0].render = lv_projectm_render;
    actor[0].vidoptions.depth = VISUAL_VIDEO_DEPTH_GL; /* We want GL clearly */

    info[0].type = const_cast<char*>(VISUAL_PLUGIN_TYPE_ACTOR);

    info[0].plugname = const_cast<char*>("projectM");
    info[0].name = const_cast<char*>("libvisual projectM");
    info[0].author = const_cast<char*>("The projectM Development Team");
    info[0].version = const_cast<char*>(PACKAGE_VERSION);
    info[0].about = const_cast<char*>("projectM");
    info[0].help = const_cast<char*>("");

    info[0].init = lv_projectm_init;
    info[0].cleanup = lv_projectm_cleanup;
    info[0].events = lv_projectm_events;

    info[0].plugin = VISUAL_OBJECT(&actor[0]);
    *count = std::size(info);

    VISUAL_VIDEO_ATTRIBUTE_OPTIONS_GL_ENTRY(actor[0].vidoptions, VISUAL_GL_ATTRIBUTE_ALPHA_SIZE, 8);
    VISUAL_VIDEO_ATTRIBUTE_OPTIONS_GL_ENTRY(actor[0].vidoptions, VISUAL_GL_ATTRIBUTE_DEPTH_SIZE, 16);
    VISUAL_VIDEO_ATTRIBUTE_OPTIONS_GL_ENTRY(actor[0].vidoptions, VISUAL_GL_ATTRIBUTE_DOUBLEBUFFER, 1);

    VISUAL_VIDEO_ATTRIBUTE_OPTIONS_GL_ENTRY(actor[0].vidoptions, VISUAL_GL_ATTRIBUTE_RED_SIZE, 8);
    VISUAL_VIDEO_ATTRIBUTE_OPTIONS_GL_ENTRY(actor[0].vidoptions, VISUAL_GL_ATTRIBUTE_GREEN_SIZE, 8);
    VISUAL_VIDEO_ATTRIBUTE_OPTIONS_GL_ENTRY(actor[0].vidoptions, VISUAL_GL_ATTRIBUTE_BLUE_SIZE, 8);
    return info;
}


/* This function is called before we really start rendering, it's the init function */
extern "C" int lv_projectm_init(VisPluginData* plugin)
{
    std::string config_file = read_config();

    // Read config file if present
    if (!config_file.empty())
    {
        ConfigFile config(config_file);

        gx = config.read<int>("Mesh X", gx);
        gy = config.read<int>("Mesh Y", gy);
        wvw = config.read<int>("Window Width", wvw);
        wvh = config.read<int>("Window Height", wvh);
        fps = config.read<int>("FPS", fps);

        transition_duration = config.read<double>("Smooth Transition Duration", transition_duration);
        preset_duration = config.read<double>("Preset Duration", preset_duration);
        hardcut_duration = config.read<double>("Hard Cut Duration", hardcut_duration);
        hardcut_sensitivity = config.read<double>("Hard Cut Sensitivity", hardcut_sensitivity);

        preset_path = config.read<std::string>("Preset Path", preset_path);
        texture_path = config.read<std::string>("Texture Path", texture_path);
    }

    fullscreen = false;

    /* Allocate the projectm private data structure, and register it as a private */
    auto* priv = new ProjectmPrivate;
    visual_mem_set(priv, 0, sizeof(ProjectmPrivate));

    //priv = visual_mem_new0 (ProjectmPrivate, 1);
    visual_object_set_private(VISUAL_OBJECT(plugin), priv);

    priv->PM = projectm_create();

    projectm_set_aspect_correction(priv->PM, true);
    projectm_set_window_size(priv->PM, wvw, wvh);
    projectm_set_mesh_size(priv->PM, gx, gy);
    projectm_set_preset_duration(priv->PM, preset_duration);
    projectm_set_hard_cut_duration(priv->PM, hardcut_duration);
    projectm_set_soft_cut_duration(priv->PM, transition_duration);
    projectm_set_fps(priv->PM, fps);

    if (!texture_path.empty())
    {
        std::vector texture_paths{texture_path.c_str()};
        projectm_set_texture_search_paths(priv->PM, texture_paths.data(), texture_paths.size());
    }

    if (priv->PM == nullptr)
    {
        return -1;
    }

    priv->PL = projectm_playlist_create(priv->PM);

    if (priv->PL == nullptr)
    {
        projectm_destroy(priv->PM);
        priv->PM = nullptr;
        return -1;
    }

    if (!preset_path.empty())
    {
        projectm_playlist_add_path(priv->PL, preset_path.c_str(), true, false);
        projectm_playlist_set_shuffle(priv->PL, true);
        projectm_playlist_play_next(priv->PL, true);
    }

    return 0;
}

extern "C" int lv_projectm_cleanup(VisPluginData* plugin)
{
    auto* priv = static_cast<ProjectmPrivate*>(visual_object_get_private(VISUAL_OBJECT(plugin)));
    visual_log_return_val_if_fail(priv != nullptr, -1);

    /* Cleanup, and thus also free our private handles */
    projectm_playlist_destroy(priv->PL);
    projectm_destroy(priv->PM);
    delete priv;
    return 0;
}

/* This is used to ask a plugin if it can handle a certain size, and if not, to
 * set the size it wants by putting a value in width, height that represents the
 * required size */
extern "C" int lv_projectm_requisition(VisPluginData* /* plugin */, int* width, int* height)
{
    *width = std::max(8, *width);
    *height = std::max(8, *height);

    return 0;
}

extern "C" int lv_projectm_dimension(VisPluginData* plugin, VisVideo* video, int width, int height)
{
    auto* priv = static_cast<ProjectmPrivate*>(visual_object_get_private(VISUAL_OBJECT(plugin)));

    visual_video_set_dimension(video, width, height);

    projectm_set_window_size(priv->PM, width, height);

    return 0;
}

/* This is the main event loop, where all kind of events can be handled, more information
 * regarding these can be found at:
 * http://libvisual.sourceforge.net/newdocs/docs/html/union__VisEvent.html
 */
extern "C" int lv_projectm_events(VisPluginData* plugin, VisEventQueue* events)
{
    auto* priv = static_cast<ProjectmPrivate*>(visual_object_get_private(VISUAL_OBJECT(plugin)));

    VisEvent ev;

    while (visual_event_queue_poll(events, &ev))
    {
        switch (ev.type)
        {
        case VISUAL_EVENT_KEYDOWN:
            switch (ev.event.keyboard.keysym.sym)
            {
            case VKEY_SPACE:
                projectm_set_preset_locked(priv->PM, !projectm_get_preset_locked(priv->PM));
                break;
            case VKEY_n:
                projectm_playlist_set_shuffle(priv->PL, false);
                projectm_playlist_play_next(priv->PL, false);
                projectm_playlist_set_shuffle(priv->PL, true);
                break;
            case VKEY_p:
                projectm_playlist_set_shuffle(priv->PL, false);
                projectm_playlist_play_previous(priv->PL, false);
                projectm_playlist_set_shuffle(priv->PL, true);
                break;
            case VKEY_r:
                projectm_playlist_play_next(priv->PL, false);
                break;
            case VKEY_BACKSPACE:
                projectm_playlist_play_last(priv->PL, false);
                break;
            case VKEY_UP:
                projectm_set_beat_sensitivity(priv->PM, projectm_get_beat_sensitivity(priv->PM) + 0.1f);
                break;
            case VKEY_DOWN:
                projectm_set_beat_sensitivity(priv->PM, projectm_get_beat_sensitivity(priv->PM) - 0.1f);
                break;

            default:
                break;
            }

        case VISUAL_EVENT_KEYUP:
            break;

        case VISUAL_EVENT_RESIZE:
            lv_projectm_dimension(plugin, ev.event.resize.video,
                                  ev.event.resize.width, ev.event.resize.height);
            break;

        default: /* to avoid warnings */
            break;
        }
    }

    return 0;
}

/* Using this function we can update the palette when we're in 8bits mode, which
 * we aren't with projectm, so just ignore :) */
extern "C" VisPalette* lv_projectm_palette(VisPluginData* plugin)
{
    return nullptr;
}

/* This is where the real rendering happens! This function is what we call, many times
 * a second to get our graphical frames. */
extern "C" int lv_projectm_render(VisPluginData* plugin, VisVideo* video, VisAudio* audio)
{
    auto* priv = static_cast<ProjectmPrivate*>(visual_object_get_private(VISUAL_OBJECT(plugin)));
    VisBuffer pcmb;
    float pcm[2][512];

    if (video->parent != nullptr)
        lv_projectm_dimension(plugin, video->parent, video->parent->width, video->parent->height);
    else lv_projectm_dimension(plugin, video, video->width, video->height);

    visual_buffer_set_data_pair(&pcmb, pcm[0], sizeof (pcm[0]));
    visual_audio_get_sample(audio, &pcmb, const_cast<char*>(VISUAL_AUDIO_CHANNEL_LEFT));

    visual_buffer_set_data_pair(&pcmb, pcm[1], sizeof (pcm[1]));
    visual_audio_get_sample(audio, &pcmb, const_cast<char*>(VISUAL_AUDIO_CHANNEL_RIGHT));

    projectm_pcm_add_float(priv->PM, reinterpret_cast<const float*>(pcm), 512, PROJECTM_STEREO);

    projectm_opengl_render_frame(priv->PM);

    return 0;
}


std::string read_config()
{
    char num[512];
    FILE* in;
    FILE* out;

    const char* home = getenv("HOME");

    // $HOME should be set. If not, skip config file and use defaults.
    if (home == nullptr || home[0] == 0)
    {
        return {};
    }

    // ~/.projectM/config.inp, used by projectM before 4.0
    std::string projectm_config_path(home);
    projectm_config_path.append("/.projectM/config.inp");

    if ((in = fopen(projectm_config_path.c_str(), "r")) != nullptr)
    {
        printf("reading ~/.projectM/config.inp \n");
        fclose(in);
        return projectm_config_path;
    }

    // New location: $XDG_CONFIG_HOME/libvisual-projectm/config.inp
    //   which is ~/.config/libvisual-projectm/config.inp by default.
    const char* xdg_config_dir = getenv("XDG_CONFIG_HOME");
    if (xdg_config_dir && xdg_config_dir[0] != 0)
    {
        projectm_config_path = xdg_config_dir;
    }
    else
    {
        projectm_config_path = home;
        projectm_config_path.append("/.config");
    }

    projectm_config_path.append("/libvisual-projectm");
    std::string projectm_config_file = projectm_config_path + "/config.inp";

    if ((in = fopen(projectm_config_file.c_str(), "r")) != nullptr)
    {
        printf("reading %s\n", projectm_config_file.c_str());
        fclose(in);
        return projectm_config_file;
    }

    // Try to create config directory
    mkdir(projectm_config_path.c_str(), 0755);

    // Create config file with default values.
    if ((out = fopen(projectm_config_file.c_str(), "w")) != nullptr)
    {
        fputs(R"(# Settings for the projectM libvisual actor plug-in

Mesh X = 48            	          # Width of per-vertex mesh
Mesh Y = 32          		      # Height of per-vertex mesh
FPS = 60          		          # Targeted frames per second (not the actual FPS!)
Window Width  = 512  	       	  # Startup window width
Window Height = 512            	  # Startup window height
Smooth Transition Duration = 3.0  # Preset transition duration in seconds
Preset Duration = 30.0 	     	  # Preset display duration in seconds
Hard Cut Duration = 10.0          # Display time until a hard cut can happen
Hard Cut Sensitivity = 10.0       # Lower to make hard cuts more frequent

# Set this to your preset dir
Preset Path = /usr/share/projectM/presets

# Set this to your textures dir
Texture Path = /usr/share/projectM/textures)", out);
        fclose(out);

        if ((in = fopen(projectm_config_file.c_str(), "r")) != nullptr)
        {
            printf("created %s successfully\n", projectm_config_file.c_str());
            fclose(in);
            return projectm_config_file;
        }
    }

    printf("Cannot create %s, skipping config file and using default values\n", projectm_config_file.c_str());
    return {};
}
