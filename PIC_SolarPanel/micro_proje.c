#include <16F877a.h>
#use delay(clock=20000000)
#include <lcd.c>
#fuses HS,WDT

#priority INT_EXT,INT_RB ,INT_TIMER2
#DEFINE SLEEP_MODE_ON 0x02
#DEFINE OTONOM_MODE_ON 0x01
#DEFINE OTONOM_MODE_OFF 0x00
int8 MODE_STATE;
/*debouncedelay deðiþkeni koymamýzýn sebebi program içerisinde yaklaþýk 10ms sayýp kesmeye gidildiðinde bir önceki kesmeden 10ms süre geçti mi bunu kontrol etmektir.
kesme içerisinde delay kullanmak doðru deðildir bu programýn diðer kesmelere girerken problem çýkarmasýna neden olabiliyor. bu yöntem sayesinde butonlardaki debounce
filtreleme olayýný yazýlýmla filtrelemiþ oluyoruz bunun diðer bir yöntemi ise donanýmsal olarak butonun baðlý olduðu devrede rc devresi eklemektir.
*/
unsigned long long debouncedelay_loopauto=2; 
unsigned long long debouncedelay_loopmanuel=0;

#BYTE STATUS = 0x03
#bit RP0 =STATUS.5
#bit RP1 =STATUS.6

/*GPIO PORT CONTROL REGISTERS*/
#BYTE TRISB = 0x86
#BYTE TRISD = 0x88
#BYTE PORTB = 0x06
#bit MODE_SELECT_PIN = PORTB.0
#bit SERVOPIN =PORTB.1
#bit RIGHT_MOVE =PORTB.2
#bit LEFT_MOVE =PORTB.3
#BYTE PORTD = 0x08

/*GENERAL INTERRUPTS CONTROL*/
#BYTE INTCON =0x0b
#bit GIE = INTCON.7
#bit PEIE = INTCON.6

/*TIMER2 INTERRUPT CONTROL REGISTERS*/
#BYTE PIE1 = 0x8c 
#bit TMR2IE=PIE1.1
#BYTE PIR = 0x0c
#bit  TMR2IF=PIR.1

/*EXTERNAL INTERRUPT CONTROL REGISTERS*/
#BYTE OPTION_REG =0x81
#bit INTEDG = OPTION_REG.6 //external interrupt edge select bit
#bit INTE =INTCON.4 //external interrupt enable/disable bit
#bit INTF =INTCON.1 //external interrupt flag 

/*TIMER2 REGISTERS*/
#BYTE T2CON = 0x12
#BYTE TMR2 = 0x11
#BYTE PR2 = 0x92

/*SERVO CONTROL VARIABLES*/
const unsigned long mindegree=100;
const unsigned long maxdegree=500;
long duty=75;
long endtime =1000;
long j=0;
long servostate=300;

/* ADC REGISTERS */
#byte ADRESL = 0x9E 
#byte ADRESH = 0x1E 
#byte ADCCON0 = 0x1F
#byte ADCCON1 = 0x9F 
#bit CHANNEL=ADCCON0.3
#bit ADCCON0_GO_DONE = ADCCON0.2 //ADCCON0->GO/DONE
#bit ADCCON0_START = ADCCON0.0//ADCCON0->ADCON
/*ADC CONTROL VARIABLES*/
 long left_ldr;
 long right_ldr;
 signed long difference; 
 
#INT_EXT
void MODE_SELECT(void)
{
INTF=0;
if(MODE_SELECT_PIN==1)
   {
      if((MODE_STATE==OTONOM_MODE_ON)&&(debouncedelay_loopauto>=3))
      {
         MODE_STATE=OTONOM_MODE_OFF;
         write_eeprom(0,OTONOM_MODE_OFF);
         debouncedelay_loopauto=0;
      }
      else if ((MODE_STATE==OTONOM_MODE_OFF)&&(debouncedelay_loopmanuel>=5))
      {
         debouncedelay_loopmanuel=0;
         MODE_STATE=OTONOM_MODE_ON;
         write_eeprom(0,OTONOM_MODE_ON);
      }
   }
   
}

#INT_TIMER2
void INT_TMR2(void)
{
TMR2IF=0; //reset interrupt flag


   if(j<endtime)
   {
      j++;
   }
   else 
   {
      j=0;
   }
   if(j<=duty)
   {
      SERVOPIN=1;
   }
   else
   {
      SERVOPIN=0;
   }
}


void main()
{
servostate=read_eeprom(1);
delay_ms(1);
servostate=servostate<<8;
servostate|=read_eeprom(2);
delay_ms(1);
duty=read_eeprom(3);

/*READ EEPROM MODE_STATE*/
if(!((read_eeprom(0)==OTONOM_MODE_ON)||(read_eeprom(0)==OTONOM_MODE_OFF)))//eepromda baþlangýçta 0 veya 1 den farklý deðer verebiliyor debug yapýldýðýnda.
{
write_eeprom(0,0);
}
MODE_STATE=read_eeprom(0);

lcd_init();
//ADC baþlatma konfigurasyonlarý
ADCCON0 = 0b01000100; 
ADCCON1 = 0b10000000; 
   
RP0=1; //bir kez bank 1'e geçmek yeterli olacaktýr.**bank0->bank1**
TRISD = 0x00;
TRISB = 0xFD;
PORTB =0x00;
PORTD=0;

INTCON = 0x00; //reset interrupt control register
GIE = 1; //enable INTCON->GIE
PEIE = 1; //enable INTCON->PEIE

INTEDG=1; //rising edge
INTF=0; //external flag reset
INTE=1; //external interrupt enable

/*RBIE=1;//exti interrupt enable
RBIF=0;//exti interrupt flag*/

T2CON=0x04;
PIE1=0x00; //reset interrupts
TMR2IE=1; //timer2 interrupts on (PIE1->TMR2IE)
PR2 = 99;
TMR2=0;
setup_wdt(WDT_2304MS);
delay_ms(5);
while(true)
{
while(MODE_STATE==OTONOM_MODE_ON)
{
restart_wdt();
   /*READ LEFT LDR VALUE*/
   CHANNEL=0;
   delay_us(20);
   ADCCON0_GO_DONE=1;
   ADCCON0_START=1;
   while(ADCCON0_GO_DONE==1);  
   left_ldr = ADRESH;
   left_ldr = left_ldr<<8;
   left_ldr |= ADRESL;
   delay_us(100);
   
   /*READ RIGHT LDR VALUE*/      
   CHANNEL=1;
   delay_us(20);
   ADCCON0_GO_DONE=1;
   ADCCON0_START=1;
   while(ADCCON0_GO_DONE==1);
   right_ldr = ADRESH;
   right_ldr = right_ldr<<8;
   right_ldr |=ADRESL;   
   delay_us(100);
   lcd_gotoxy(1,1);
   printf(lcd_putc,"\fOTONOM MODE");
   lcd_gotoxy(1,2); 
   printf(lcd_putc,"SOL=%Lu SAG=%Lu   ",left_ldr,right_ldr);
   delay_ms(1);
   
/*AUTONOM SERVO DRIVE ALGORITHMA.*/   
   difference = left_ldr-right_ldr; //eðer fark negatifse ýþýk sað taraftan vuruyor anlamýna gelir.pozitife çevirip algoritmayý yazýyorum.
  
   if(difference <0)
   {
      difference=difference*-1;
      if (difference<200)
      {
      //tolerans aralýðý servostate deðiþkeni ldr sensörleri arasýndaki fark belli bir deðerden az ise deðiþmeyecektir.
      }
      else if((difference>=200)&&(difference <=1024))
      {
         servostate++;
      }
      else
      {
         printf(lcd_putc,"\f");
         printf(lcd_putc,"  ERROR  ");
         delay_ms(1);
      }
   }
   else if (difference >=0)
   {
      if(difference<200)
      {
       //tolerans aralýðý servostate deðiþkeni ldr sensörleri arasýndaki fark belli bir deðerden az ise deðiþmeyecektir.
      }
      else if((difference>=200)&&(difference <=1024))
      {
         servostate--;
      }
      else
      {
         printf(lcd_putc,"\f");
         printf(lcd_putc,"  ERROR  ");
         delay_ms(1);
      }
   
   }
   if(servostate>=maxdegree)
   {
      servostate=maxdegree;
      duty = servostate/4;
   }
   else if(servostate<=mindegree)
   {
      servostate=mindegree;
      duty = servostate/4;
   }
   else 
   {
      duty=servostate/4;
   }
   debouncedelay_loopauto++;
   if(debouncedelay_loopauto==4294967296-1) //debounce filter sayacýný sýfýrlamýþ oluyoruz.
   {
      debouncedelay_loopauto=3;
   }
   /*servo durumunu kayýt eder*/
   write_eeprom(1,make8(servostate,1));
   delay_ms(2);
   write_eeprom(2,make8(servostate,0));
   delay_ms(2);
   Write_eeprom(3,duty);
   
}
while(MODE_STATE==OTONOM_MODE_OFF)
{
restart_wdt();
   
   if(RIGHT_MOVE)
   {
      servostate++;
      if(servostate>=maxdegree)
      {
         servostate=maxdegree;
      }
     
   }
   else if(LEFT_MOVE)
   {
      servostate--;
      if(servostate<=mindegree)
      {
         servostate=mindegree;
      }
     
   }
   duty=servostate/4;
   lcd_gotoxy(1,1); 
    printf(lcd_putc,"\fMANUEL MODU");
    lcd_gotoxy(1,2);
   printf(lcd_putc,"SERVOSTATE = %Lu",servostate);
   debouncedelay_loopmanuel++;
   if(debouncedelay_loopmanuel==4294967296-1)
   {
      debouncedelay_loopmanuel=2;
   }
   /*servo durumunu kayýt eder*/
   write_eeprom(1,make8(servostate,1));
   delay_ms(1);
   write_eeprom(2,make8(servostate,0));
   delay_ms(1);
   Write_eeprom(3,duty);
  }

}
}
