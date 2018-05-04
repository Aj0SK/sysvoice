#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>

#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define SAMPLING 24100
#define FRAMING 32

// nc -l 2234 -u
// nc -u localhost 2334

int rec_desc, send_desc;
char * adresa = "127.0.0.1";
int cielport = 2234;
int moj_port = 2334;

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


void f(snd_pcm_t **handle)
{
    unsigned int sampling_rate = SAMPLING;
    int rc, dir;
    
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames = FRAMING;
    
    rc = snd_pcm_open(&(*handle), "default", SND_PCM_STREAM_CAPTURE, 0);
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

int main()
{
    snd_pcm_t *handle;
    snd_pcm_uframes_t frames = FRAMING;
    
    f(&handle);
    
    //////////////////////siet
    
    send_desc = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in komu;
    komu.sin_family = AF_INET;
    komu.sin_addr.s_addr = inet_addr(adresa);
    komu.sin_port = htons( cielport );
    socklen_t komulen = sizeof(komu);

    rec_desc = socket( PF_INET , SOCK_DGRAM, 0);
    struct sockaddr_in me;
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = INADDR_ANY;
    me.sin_port = htons(moj_port);
    
    if ( bind(rec_desc,  (struct sockaddr*)&me, sizeof(me) )  == -1)
    {
        perror("Neuspesny bind.");
        exit(1);
    }

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
    
    while (1)
    {
        fd_set set;

        FD_ZERO(&set);
        FD_SET(fileno(stdin), &set );

        int ret = select( fileno( stdin )+1, &set, NULL, NULL, &tv );

        if(ret > 0)
        {
            fprintf(fp, "je to dobre stlacene %c\n", x);
            read( fileno( stdin ), &x, 1 );
            break;
        }
        
        rc = snd_pcm_readi(handle, sound_buffer, frames);
        
        if (rc == -EPIPE)
        {
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        } 
        else if (rc < 0) 
        {
            fprintf(stderr, "error from read: %s\n",
            snd_strerror(rc));
        }
        else if (rc != (int)frames) fprintf(stderr, "short read, read %d frames\n", rc);
    
        rc = write(1, sound_buffer, sound_buffer_size);
        
        //sendto(send_desc, sound_buffer, sound_buffer_size, 0, (struct sockaddr *)&komu , komulen);
        
        fd_set rec_set;

        FD_ZERO(&rec_set);
        FD_SET(rec_desc, &rec_set );
        
        ret = select( rec_desc+1, &rec_set, NULL, NULL, &tv );
        
        if(ret > 0)
        {
            char buf[1001];
            ret = recvfrom(rec_desc, buf, 1000, 0, (struct sockaddr*)&odkial, &velkost);
            buf[ret]=0;
            fprintf(stderr, "Od [%s] som dostal som \"%s\"\n",inet_ntoa(odkial.sin_addr),buf);
            break;
        }

        if (rc != sound_buffer_size) fprintf(stderr, "short write: wrote %d bytes\n", rc);
    }
    
    reset_input_mode();
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(sound_buffer);
    fclose(fp);
    
    close(send_desc);
    close(rec_desc);
    
    return 0;
}