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

#define SAMPLING 24000
#define FRAMING 16

char * adresa = NULL;
int port = 3333;

struct termios saved_attributes;

void set_input_mode(void)
{
    struct termios newSettings;

    tcgetattr(fileno(stdin), &saved_attributes);
    newSettings = saved_attributes;
    newSettings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(fileno(stdin), TCSANOW, &newSettings);
}

void reset_input_mode(void)
{
    tcsetattr( fileno( stdin ), TCSANOW, &saved_attributes);
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
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any((*handle), params);
    snd_pcm_hw_params_set_access((*handle), params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format((*handle), params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels((*handle), params, 2);
    snd_pcm_hw_params_set_rate_near((*handle), params, &sampling_rate, &dir);
    snd_pcm_hw_params_set_period_size_near((*handle), params, &frames, &dir);

    rc = snd_pcm_hw_params((*handle), params);

    if (rc < 0)
    {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }
    
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    snd_pcm_hw_params_get_period_time(params, &sampling_rate, &dir);
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "Maly pocet parametrov. Zadajte prosim cielovu IP adresu.");
        exit(1);
    }
    else adresa = argv[1];
    
    if(argc > 2)
    {
        port = atoi(argv[2]);
    }
    
    int rec_desc, send_desc;
    
    snd_pcm_t *cap_handle, *play_handle;
    snd_pcm_uframes_t frames = FRAMING;
    
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
    
    if ( bind(rec_desc,  (struct sockaddr*)&me, sizeof(me) )  == -1)
    {
        fprintf(stderr, "Neuspesny bind.");
        exit(1);
    }

    fprintf(stderr, "Pripojeny na %s cez port %d\n", adresa, port);
    
    struct sockaddr_in odkial;
    socklen_t velkost = sizeof(odkial);

    /////
    
    set_input_mode();
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    FILE *fp = fopen("error.txt", "wb");
    unsigned char x;
    
    int rc, sound_buffer_size = 4 * FRAMING;
    char *sound_buffer = (char *) malloc(sound_buffer_size);
    
    bool nahravam = false;
    char mega[1000000];
    
    while (1)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fileno(stdin), &set );
        int ret = select( fileno( stdin )+1, &set, NULL, NULL, &tv );
        if(ret > 0) // program nahrava/nenahrava pri stlaceni klavesu
        {
            read( fileno( stdin ), &x, 1 );
            nahravam = !nahravam;
        }
        
        rc = snd_pcm_readi(cap_handle, sound_buffer, frames);
        
        if (rc == -EPIPE)
        {
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(cap_handle);
        } 
        else if (rc < 0) 
        {
            fprintf(stderr, "error from read: %s\n",
            snd_strerror(rc));
        }
        else if (rc != (int)frames) fprintf(stderr, "short read, read %d frames\n", rc);
    
        rc = write(1, sound_buffer, sound_buffer_size);
        if (rc != sound_buffer_size) fprintf(stderr, "short write: wrote %d bytes\n", rc);
        
        if(nahravam) sendto(send_desc, sound_buffer, sound_buffer_size, 0, (struct sockaddr *)&komu , komulen);
        
        fd_set rec_set;

        FD_ZERO(&rec_set);
        FD_SET(rec_desc, &rec_set );
        
        ret = select( rec_desc+1, &rec_set, NULL, NULL, &tv );
        if(ret > 0)
        {
            ret = recvfrom(rec_desc, mega, 4*FRAMING, 0, (struct sockaddr*)&odkial, &velkost);
            mega[ret] = 0;
            
            rc = snd_pcm_writei(play_handle, mega, frames);
            
            if (rc == -EPIPE)
            {
                fprintf(stderr, "underrun occurred\n");
                snd_pcm_prepare(play_handle);
            }
            else if (rc < 0) fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
            else if (rc != (int)frames) fprintf(stderr, "short write, write %d frames\n", rc);
        }

    }

    reset_input_mode();
    snd_pcm_drain(cap_handle);
    snd_pcm_close(cap_handle);
    snd_pcm_drain(play_handle);
    snd_pcm_close(play_handle);
    free(sound_buffer);
    fclose(fp);
    
    close(send_desc);
    close(rec_desc);
    
    return 0;
}