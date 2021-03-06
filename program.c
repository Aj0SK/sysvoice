#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>

#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>

int SAMPLING = 24000;
int FRAMING = 16;

char * adresa = NULL;
int port = 3333;

struct termios saved_attributes;
int er;

void show_error(char * message)
{
    fprintf(stderr, "FATAL ERROR !!! : %s\n", message);
    exit(1);
}

void set_input_mode(void)
{
    struct termios newSettings;

    er = tcgetattr(fileno(stdin), &saved_attributes);
    if(er == -1) show_error("Nastala chyba pri ziskavani parametrov terminalu.");
    
    newSettings = saved_attributes;
    newSettings.c_lflag &= (~ICANON & ~ECHO);
    
    er = tcsetattr(fileno(stdin), TCSANOW, &newSettings);
    if(er == -1) show_error("Nastala chyba pri ziskavani parametrov terminalu.");
}

void reset_input_mode(void)
{
    er = tcsetattr( fileno( stdin ), TCSANOW, &saved_attributes);
    if(er == -1) show_error("Nastala chyba pri nastavovani parametrov terminalu.\n");
}

void f(snd_pcm_t **handle, snd_pcm_stream_t type)
{
    unsigned int sampling_rate = SAMPLING;
    int rc, dir;
    
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames = FRAMING;
    
    rc = snd_pcm_open(&(*handle), "default", type, 0);
    if (rc < 0)
    {
        fprintf(stderr, "SOUND : unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any((*handle), params);
    snd_pcm_hw_params_set_access((*handle), params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format((*handle), params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels((*handle), params, 2);
    snd_pcm_hw_params_set_rate_near((*handle), params, &sampling_rate, &dir);
    snd_pcm_hw_params_set_period_size_near((*handle), params, &frames, &dir);

    FRAMING = frames;
    SAMPLING = sampling_rate;
    
    rc = snd_pcm_hw_params((*handle), params);

    if (rc < 0)
    {
        fprintf(stderr, "SOUND : unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    printf("Realne parametre zvuku su : SAMPLES(%d) a FRAMES(%d) pre %s\n", SAMPLING, FRAMING, 
           (type == SND_PCM_STREAM_PLAYBACK)?("prehravanie"):("nahravanie"));
    
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    snd_pcm_hw_params_get_period_time(params, &sampling_rate, &dir);
}

int main(int argc, char **argv)
{
    if(argc < 2) show_error("Maly pocet parametrov. Zadajte prosim cielovu IP adresu.");
    else adresa = argv[1];
    if(argc > 2) port = atoi(argv[2]);
    
    int rec_desc, send_desc;
    
    snd_pcm_t *cap_handle, *play_handle;

    
    f(&play_handle, SND_PCM_STREAM_PLAYBACK);
    f(&cap_handle, SND_PCM_STREAM_CAPTURE);
    
    //////////////////////siet
    
    send_desc = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in komu;
    komu.sin_family = AF_INET;
    komu.sin_addr.s_addr = inet_addr(adresa);
    komu.sin_port = htons( port );
    socklen_t komulen = sizeof(komu);

    rec_desc = socket(PF_INET , SOCK_DGRAM, 0);
    struct sockaddr_in me;
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = INADDR_ANY;
    me.sin_port = htons(port);
    
    if ( bind(rec_desc,  (struct sockaddr*)&me, sizeof(me) )  == -1) show_error("Neuspesny bind.");

    printf("Pripojeny na %s cez port %d, ukoncite klavesom q\n", adresa, port);
    
    struct sockaddr_in odkial;
    socklen_t velkost = sizeof(odkial);

    /////
    
    set_input_mode();
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    unsigned char x;
    
    int rc, sound_buffer_size = 2 * 2 * FRAMING;
    char *sound_buffer = (char *) malloc(sound_buffer_size);
    
    bool nahravam = false;
    char mega[sound_buffer_size];
    
    while (1)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fileno(stdin), &set );
        int ret = select( fileno( stdin )+1, &set, NULL, NULL, &tv );
        if(ret > 0) // program nahrava/nenahrava pri stlaceni klavesu
        {
            read( fileno( stdin ), &x, 1 );
            
            if(x == 'q') // ukoncenie programu pri stlaceni q
            {
                printf("Ukoncujem program.\n");
                break;
            }
            
            nahravam = !nahravam;
        }
        
        rc = snd_pcm_readi(cap_handle, sound_buffer, FRAMING);
        
        if (rc == -EPIPE)
        {
            fprintf(stderr, "SOUND : overrun occurred\n");
            snd_pcm_prepare(cap_handle);
        } 
        else if (rc < 0) 
        {
            fprintf(stderr, "SOUND : error from read: %s\n",
            snd_strerror(rc));
        }
        else if (rc != FRAMING) fprintf(stderr, "SOUND : short read, read %d frames\n", rc);
    
        //rc = write(1, sound_buffer, sound_buffer_size);
        //if (rc != sound_buffer_size) fprintf(stderr, "short write: wrote %d bytes\n", rc);
        
        if(nahravam) sendto(send_desc, sound_buffer, sound_buffer_size, 0, (struct sockaddr *)&komu , komulen);
        
        fd_set rec_set;

        FD_ZERO(&rec_set);
        FD_SET(rec_desc, &rec_set );
        
        ret = select( rec_desc+1, &rec_set, NULL, NULL, &tv );
        if(ret > 0)
        {
            ret = recvfrom(rec_desc, mega, sound_buffer_size, 0, (struct sockaddr*)&odkial, &velkost);
            if(ret == -1 ) continue;
            
            rc = snd_pcm_writei(play_handle, mega, FRAMING);
            
            if (rc == -EPIPE)
            {
                fprintf(stderr, "SOUND : underrun occurred\n");
                snd_pcm_prepare(play_handle);
            }
            else if (rc < 0) fprintf(stderr, "SOUND : error from writei: %s\n", snd_strerror(rc));
            else if (rc != FRAMING) fprintf(stderr, "SOUND : short write, write %d frames\n", rc);
        }

    }

    reset_input_mode();
    snd_pcm_drain(cap_handle);
    snd_pcm_close(cap_handle);
    snd_pcm_drain(play_handle);
    snd_pcm_close(play_handle);
    free(sound_buffer);
    
    close(send_desc);
    close(rec_desc);
    
    return 0;
}