#include <stdbool.h>
#include "wrapper.h"


/*
 * hlavicka pre uchovavani inforamcii o nasledujucom bloku
 */

// magicka premenna pre identifikaciu uplne prvej hlavicky
#define checkHlavicky 666
// magicka premenna pre zistovanie neexistujuceho predosleho bloku
#define neexistPredoslyBlok -1
// hlavicka samotna
typedef struct {
    unsigned int magic, velkost, nenalokovane, predosly ;
    bool prazdne;
} __attribute__((__packed__)) hlavicka;
// cast coho na co
#define CAST(__What, __To) ((__To)(__What))

/*
 *  funkcia na zapis hlavicky po 1B pomocou mwrite() z wrappera
 */
void zapisHlavicky(hlavicka* hlavicka, int pozicia){
    // precastim hlavicku na
    uint8_t* spracovavanaHlavicka = CAST(hlavicka, uint8_t*);
    // cyklom prejdem celu hlavicku
    for(int i = 0; i < sizeof(hlavicka); i++, pozicia++){
        mwrite(pozicia, spracovavanaHlavicka[i]);
    }
}
void citajHlavicku(hlavicka* hlavicka, int pozicia){
    // precastim hlavicku na
    uint8_t* spracovavanaHlavicka = CAST(hlavicka, uint8_t*);
    // cyklom prejdem celu hlavicku
    for(int i = 0; i < sizeof(hlavicka); i++, pozicia++){
        spracovavanaHlavicka[i] = mread(pozicia);
    }
}


/*
 * Inicializacia pamate
 *
 * Zavola sa, v stave, ked sa zacina s prazdnou pamatou, ktora je inicializovana
 * na 0.
 KOMENTAR PRI RIESENIE

 m_init bude robit nasledovne:
 	vytovri hlavicku
 		magic = 666
 		velkost = m_size - sizeOf(hlavicka)
 		prazdny = true
		nealokovane = 0
		predosly = invalid
 */
void my_init(void) {
    hlavicka prvotnaHlavicka = {
            .magic = checkHlavicky,
            .nenalokovane = 0,
            .velkost= msize() - sizeof(hlavicka),
            .prazdne = true,
            .predosly = neexistPredoslyBlok
    };
    zapisHlavicky(&prvotnaHlavicka, 0);
	return;
}

 /*
  * najdiMiestoPreHlavicku
  *     zacne na zaciatku pamate
  *     prejde celu pamat po hlavickach
  *     ked najde hlavicku pre blok, ktory vyhovuje (tmpHlavicka.prazdne && tmpHlavicka.velkost >= velkost)
  *         tak vrati poziciu poziciu tejto hlavicky
  *         //najde prvy vhodny usek
  *     ak sa nepodari najst vhodnu hlavicku, tak vrati FAIL
  */
int najdiMiestoPreHlavicku(unsigned int velkost){
    hlavicka tmpHlavicka;
    unsigned int pozicia = 0;
    while(pozicia < msize()){
        citajHlavicku(&tmpHlavicka, pozicia);
        if (tmpHlavicka.prazdne && tmpHlavicka.velkost >= velkost){
            return pozicia;
        }
        pozicia += sizeof(hlavicka) + tmpHlavicka.velkost + tmpHlavicka.nenalokovane;
    }
    return FAIL;
}
 /*
  * m_alloc(velkost) spravi nasledujuce veci:
  *     do r nacita hlavicku, ktora je vhodna pre alokovanie
  * z hladiska jednoduchosti ignorujeme mozne nenaalokvane predchadzajuceho bloku - kod mi sa skomplikoval a vznikli by miesta na chyby ========== DOLEZITY KOMENTAR
  */
int my_alloc(unsigned int size) {
     /* Nemozeme alokovat viac pamate, ako je dostupne */
     if (size >= msize() - 1)
         return FAIL;
     // najdem si poziciu hlavicky pre blok, ktory vyhovuje size
     int poziciaHlavicky = najdiMiestoPreHlavicku( size );

     //tu uz mozem ocakavat, ze som dostal korektnu poziciu hlavicky
     //vytvorim si novu hlavicku
     hlavicka tmpHlavicka;
     //do nej nacitam hlavicku pre blok, ktory je dostatocne velky pre ziadany size
     citajHlavicku(&tmpHlavicka, poziciaHlavicky);
     //do buducna sa mi moze hodit pozicia nasledujucej hlavicky
     unsigned int nasledujucaHlavickaPozicia = poziciaHlavicky + sizeof(hlavicka) + tmpHlavicka.velkost;
     //hlavicku pre ziadanu velkost upravim tak, aby znacila, ze blok, ktory po nej nasleduje je uz alokovany
     tmpHlavicka.velkost = size;
     tmpHlavicka.prazdne = false;
     //ak mam dost miesta na vytvorenie novej hlavicky v bloku za size
     if(tmpHlavicka.velkost >= size + sizeof(hlavicka)){
         // vytvorim novu hlavicku
         hlavicka novaHlavicka;
         // velkost bloku, ktory bude za nou je tmpHlavicka.velkost - (size + sizeof(hlavicka)) velky
         novaHlavicka.velkost = tmpHlavicka.velkost - (size + sizeof(hlavicka));
         // je prazdny, teda sa moze v buducnosti alokovat
         novaHlavicka.prazdne = true;
         novaHlavicka.nenalokovane = 0;
         // nova hlavicka ma pred sebou blok, ktorej hlavicka sa nachadza na pozicii, ktoru nam vratila funkcia iMiestoPreHlavicku
         novaHlavicka.predosly = poziciaHlavicky;
         // zapisem tuto novu hlavicku do te
         zapisHlavicky(&novaHlavicka, poziciaHlavicky + sizeof(hlavicka) + size);

         // upravim hlavicku za blokom, ktory som prave naalokoval
         // vytvorim si novu hlavicku
         hlavicka nasledujucaHlavicka;
         // nacitam do nej udaje
         citajHlavicku(&nasledujucaHlavicka, nasledujucaHlavickaPozicia);
         // zmenim poziciu hlavicky bloku, ktory predchadza tejto hlavicke na poziciu hlavicky, ktoru som vyssie vytvoril
         nasledujucaHlavicka.predosly = poziciaHlavicky + sizeof(hlavicka) + size;
         // zapisem zmeny v nasledujucej hlavicke
         zapisHlavicky(&nasledujucaHlavicka, nasledujucaHlavickaPozicia);
     }
     // v bloku je sice dost miesta na alokovanie size, ale nie je dost miesta nato, aby sme vytvorili novu hlavicku = zostanene nam nenalokovatelne miesto
     else {
         tmpHlavicka.nenalokovane = tmpHlavicka.velkost - size;
     }
     // zapisem zmeny povodnej hlavicky
     zapisHlavicky(&tmpHlavicka, poziciaHlavicky);
     // vratim adresu prveho B bloku, ktory som prave naalokoval
     return poziciaHlavicky + sizeof(hlavicka);
}


// spoji dva bloky do jedneho a prepise hlavicku prveho tak, aby zodpovedala bloku, ktory po nej v pamati nasleduje
// predpokladam, ze oba bloky su prazdne = neodalokujem nieco, co som nechcel odalokovat
void spojDvaBloky(unsigned int poziciaPrvejHlavicky){
    hlavicka prvaHlavicka;
    citajHlavicku(&prvaHlavicka, poziciaPrvejHlavicky);
    hlavicka druhaHlavicka;
    citajHlavicku(&druhaHlavicka, poziciaPrvejHlavicky + sizeof(hlavicka) + prvaHlavicka.velkost + prvaHlavicka.nenalokovane);

    int moznaPoziciaTretejHlavicky = poziciaPrvejHlavicky
                                     + sizeof(hlavicka) + prvaHlavicka.velkost + prvaHlavicka.nenalokovane
                                     + sizeof(hlavicka) + druhaHlavicka.velkost + druhaHlavicka.nenalokovane;
    // ak za druhou hlavickou je este jedna hlavicka, tak jej musim prepisat adresu predoslej hlavicky
    if(moznaPoziciaTretejHlavicky < msize()){
        hlavicka tretiaHlavicka;
        citajHlavicku(&tretiaHlavicka, moznaPoziciaTretejHlavicky);
        tretiaHlavicka.predosly = poziciaPrvejHlavicky;
    }
    prvaHlavicka.prazdne = true;
    prvaHlavicka.velkost += prvaHlavicka.nenalokovane + sizeof(hlavicka) + druhaHlavicka.velkost + druhaHlavicka.nenalokovane;


}

int my_free(unsigned int addr) {
    if (addr > sizeof(hlavicka) && addr < msize()) {
        hlavicka uvolniHlavicka, vpravoHlavicka, vlavoHlavicka;
        citajHlavicku(&uvolniHlavicka, addr - sizeof(hlavicka));
        // uvolnit mozem bezohladu od okolitych blokov
        uvolniHlavicka.prazdne = true;




        // tu budem skusat spajat vedlajsie bloky s aktualnym blokom

        // najprv sa pozriem, dolava, ci je nejaky vedlajsi blok
        if (uvolniHlavicka.predosly != -1) citajHlavicku(&vlavoHlavicka, uvolniHlavicka.predosly);

        // potom sa pozriem na hlavicku vpravo
        unsigned int poziciaVpravo = addr + uvolniHlavicka.velkost + uvolniHlavicka.nenalokovane;
        if (poziciaVpravo < msize()) citajHlavicku(&vpravoHlavicka, poziciaVpravo);


        // ak je blok vlavo prazdny, tak ho spojim s tymto
        if (vlavoHlavicka.prazdne) {
            unsigned addr = uvolniHlavicka.predosly;
            spojDvaBloky(uvolniHlavicka.predosly);
            if (vpravoHlavicka.prazdne) spojDvaBloky(addr);
        }
        //inak skusim spojit s tym vedla ak je to prazdny blok
        if (vpravoHlavicka.prazdne) spojDvaBloky(addr);

        return OK;
    } else
        return FAIL;

}
