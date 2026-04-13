#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "rsm.h"

// --- SISTEM YAPILANDIRMASI (System Configuration) ---
#define NUMR 1        // Sistemdeki kaynak tipi (resource type) sayisi. Sadece R0 var.
#define NUMP 2        // Sistemde calisacak toplam process (child) sayisi.

int AVOID = 1;        // Varsayilan olarak deadlock avoidance acik. (Main icinde arg ile degisir)
int exist[1] =  {8};  // R0 tipinden sistemde bastan var olan miktar (Ornegin 8 adet R0 kaynagi).

// --- YARDIMCI FONKSIYON: pr (Yazdirma) ---
// Bir process'in yaptigi istegi terminale formatli sekilde yazdirir.
void pr (int apid, char astr[], int m, int r[])
{
    int i;
    printf ("process %d, %s, [", apid, astr);
    for (i = 0; i < m; ++i) {
        if (i == (m-1))
            printf ("%d", r[i]);
        else
            printf ("%d,", r[i]);
    }
    printf ("]\n");
}

// --- YARDIMCI FONKSIYON: setarray (Dizi Atama) ---
// Icerisine verilen argumanlari tek bir satirda diziye atamaya yarar (Coklu atama kolayligi).
void setarray (int r[MAX_RT], int m, ...)
{
    va_list valist;
    int i;
    
    va_start (valist, m);
    for (i = 0; i < m; i++) {
        r[i] = va_arg(valist, int);
    }
    va_end(valist);
    return;
}

// --- CHILD PROCESS 1 (apid = 0) ---
void func_p1 (int apid)
{
    int request1[MAX_RT];
    int request2[MAX_RT];
    int claim[MAX_RT];
    
    // Kutuphaneye "Ben basladim!" bildirisini yapiyoruz. (Barrier burada bekletebilir)
    rsm_process_started (apid);

    // Maximum ihtiyaci bastan sisteme bildiriyoruz (Sadece Avoidance modunda dikkate alinir)
    setarray(claim, NUMR, 8); // P1 toplamda maksimum 8 adet R0 isteyebilir
    rsm_claim (claim);
    
    // 1. ISTEK: 5 adet R0 istiyoruz. Eger avoidance kapaliysa ve bostaysa hemen aliriz.
    // Avoidance aciksa safe-state kontrolunden mecburi gecer.
    setarray(request1, NUMR, 5);
    pr (apid, "REQ", NUMR, request1);
    rsm_request (request1);

    // 4 Saniye boyunca kaynak bende (Sleep: burada isi yapiyor gibi simule ediyoruz)
    sleep(4);

    // 2. ISTEK: Geri kalan islem icin ek 3 adet R0 daha istiyoruz.
    setarray(request2, NUMR, 3);
    pr (apid, "REQ", NUMR, request2);
    rsm_request (request2);

    // Isimiz bittiginde tum aldiklarimizi sisteme iade ediyoruz.
    rsm_release (request1);
    rsm_release (request2);

    // Kutuphaneye "Benim islerim bitti, cikiyorum" diyoruz.
    rsm_process_ended();
    exit(0);
}

// --- CHILD PROCESS 2 (apid = 1) ---
void func_p2 (int apid)
{
    int request1[MAX_RT];
    int request2[MAX_RT];
    int claim[MAX_RT];

    // Sisteme kayit oluyoruz
    rsm_process_started (apid);

    // Maximum ihtiyaci bildiriyoruz
    setarray(claim, NUMR, 8); // P2 de toplamda maksimum 8 adet R0 talep edebilir.
    rsm_claim (claim);

    // 1. ISTEK: Hemen 2 adet R0 istiyoruz.
    setarray(request1, NUMR, 2);
    pr (apid, "REQ", NUMR, request1);
    rsm_request (request1);

    // 2 saniye kaynagi kullan
    sleep(2);
    
    // 2. ISTEK: Ek olarak 4 adet R0 istiyoruz.
    setarray(request2, NUMR, 4);
    pr (apid, "REQ", NUMR, request2);
    rsm_request (request2);

    // Kaynaklari geri listesinden sildir (Release).
    rsm_release (request1);
    rsm_release (request2);

    // Islemi sonlandir.
    rsm_process_ended ();
    exit(0);
}

// --- ANA (MAIN) PROCESS ---
int main(int argc, char **argv)
{
    int i;
    int count;
    int ret;
    int n;

    // "./app <avoidflag>" formati disari atilmasini engelliyor
    if (argc != 2) {
        printf ("usage: ./app avoidflag\n");
        exit (1);
    }

    AVOID = atoi (argv[1]);
    
    // RSM kutuphanesini (Shared Memory ve Semaforlari) baslatiyoruz (0=Detection, 1=Avoidance)
    if (AVOID == 1)
        rsm_init (NUMP, NUMR, exist, 1);
    else
        rsm_init (NUMP, NUMR, exist, 0);

    // P1 icin fork atalim ve onu func_p1 fonksiyonuna baglayalim. 
    // apid degeri 0 olacak.
    i = 0;  // P1 apid=0
    n = fork();
    if (n == 0) {
        func_p1(i);
    }

    // P2 icin fork atalim ve onu func_p2 fonksiyonuna baglayalim.
    // apid degeri 1 olacak.
    i = 1;  // P2 apid=1
    n = fork();
    if (n == 0) {
        func_p2(i);
    }

    // --- ANA PROCESS DONGUSU (Monitoring & Detection) ---
    count = 0;
    int deadlock_found = 0;
    
    // Ana process, sistemin gidisatini gozlemler. (Maks 10 saniye boyunca denetim)
    while (count < 10) {
        sleep(1);

        // Sistemin canli (matris) durumunu ekrana yazdiralim
        rsm_print_state("The Current State of the System");

        // Deadlock kontrolu yapiyoruz
        ret = rsm_detection();
        if (ret > 0) {
            printf("DEADLOCK DETECTED, count=%d\n", ret);
            rsm_print_state("State after deadlock");
            deadlock_found = 1;
            break; // Kilitlenme varsa Donguden kiril
        }
        count++;
    }

    if (deadlock_found) {
        // Eger deadlock varsa, durumu gosterip grupdaki herkesi zorla kapatiyoruz.
        printf("Deadlock found. Terminating group...\n");
        rsm_print_state("State after deadlock");
        rsm_destroy(); // SHM bellegi temizle
        kill(0, SIGTERM); // Tum cocuklari oldur
        exit(0);
    } else {
        // Herhangi bir kilitlenme olmadiysa (Ozellikle Avoidance modu 1 iken buraya duser), 
        // Cocuk process'lerin duzgunce islerini bitirip kapanmalarini bekle.
        for (i = 0; i < NUMP; ++i) {
            wait(NULL);
        }
    }

    // Basariyla bittiyse (Herkes isini tamamlayip resources'lari release edip ciktiysa) 
    // paylasilan bellegi (SHM) son defa temizle.
    rsm_destroy();
    return 0;
}

