��    +      t  ;   �      �     �      �     �          +     H      f     �     �  $   �  #   �       )   "     L  -   h     �  "   �  !   �     �          3     P     p      �     �     �     �     �          5     Q     q     �  !   �  U   �  T   )  D   ~  0   �  :   �     /	  P   @	  7   �	     �	  l   �  }   W  �   �  �   �  �   &  �   �  ~   x  n  �  W   f  �   �  c   w  �  �  �   �     �  �   �  �   M  �   �  �   �  �   R  m     �   r  �  Y  �   8  C  �  �  4  �   !    	"  m   &#  i   �#  ~   �#  [   }$  a   �$  �   ;%  B   �%  f    &  d   g&  Z   �&  J   ''  0   r'     �'  _   �'  ;   (     	                      +   &              %   )              $             #                                     '              (                           "             
   !            *                 !ContextTip:ChangingNTFS !ContextTip:DirPicker:UseDefault !ContextTip:Folders:Logs !ContextTip:Folders:Savestates !ContextTip:Folders:Settings !ContextTip:Folders:Snapshots !ContextTip:Framelimiter:Disable !ContextTip:GS:DisableOutput !ContextTip:GS:SyncMTGS !ContextTip:Gamefixes:EE Timing Hack !ContextTip:Gamefixes:OPH Flag hack !ContextTip:Speedhacks:BIFC0 !ContextTip:Speedhacks:EECycleRate Slider !ContextTip:Speedhacks:INTC !ContextTip:Speedhacks:VUCycleStealing Slider !ContextTip:Speedhacks:fastCDVD !ContextTip:Speedhacks:vuBlockHack !ContextTip:Speedhacks:vuFlagHack !ContextTip:Window:Fullscreen !ContextTip:Window:HideGS !ContextTip:Window:HideMouse !ContextTip:Window:ManagedVsync !ContextTip:Window:Vsync !Notice:Tooltip:Presets:Checkbox !Notice:Tooltip:Presets:Slider !Panel:HasHacksOverrides !Panel:HasPluginsOverrides !Panel:Speedhacks:EECycleX1 !Panel:Speedhacks:EECycleX2 !Panel:Speedhacks:EECycleX3 !Panel:Speedhacks:VUCycleSteal1 !Panel:Speedhacks:VUCycleSteal2 !Panel:Speedhacks:VUCycleSteal3 !Panel:Speedhacks:VUCycleStealOff Completely disables all GS plugin activity; ideal for benchmarking EEcore components. For troubleshooting potential bugs in the MTGS only, as it is potentially very slow. Includes idle event processing and some other uncommon event usages. Internal Memorycard Plugin failed to initialize. Logs events as they are passed to the PS2 virtual machine. No reason given. The MTGS thread has become unresponsive while waiting for the GS plugin to open. Threading activity: start, detach, sync, deletion, etc. Project-Id-Version: PCSX2 0.9.8
Report-Msgid-Bugs-To: http://code.google.com/p/pcsx2/
POT-Creation-Date: 2011-04-01 20:20+0200
PO-Revision-Date: 2011-03-25 19:23+0100
Last-Translator: Georg Ronnert <pgert@yahoo.se>
Language-Team: 
Language: 
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 8bit
X-Poedit-KeywordsList: pxEt;pxLt
X-Poedit-SourceCharset: utf-8
X-Poedit-Basepath: C:\Arkiv\PCSX2\SVN
X-Poedit-Language: Swedish
X-Poedit-Country: SWEDEN
X-Poedit-SearchPath-0: pcsx2
X-Poedit-SearchPath-1: common
 NTFS komprimering kan ändras manuellt när som helst genom att använda filegenskaper hos Windows Explorer. När markerad kommer denna mapp att automatiskt begrunda förvalen assosierade med PCSX2's nuvarande användarinställningar. I denna mapp sparar PCSX2 sina loggfiler och diagnostiska rapporter. De flesta insticksprogram 
håller fast vid denna mapp, men dock kan vissa äldre insticksprogram ignorera den. I denna mapp sparar PCSX2 sparpunkter, vilka är sparade antingen genom användning 
av menyer/verktygsrad, eller genom att trycka på F1/F3 (spara/ladda). Detta är mappen där PCSX2 sparar Era inställningar, och då även inställningar skapade 
av de flesta insticksprogram (gäller måhända dock inte för vissa äldre insticksprogram). I denna mapp sparar PCSX2 skärmbilder. Det faktiska skärmbildsformatet och stilen 
kan variera beroende på vilket GS-insticksprogram som användes. Bemärk att när Bildbegränsning är förhindrad så kommer Turbo och SlowMotion lägena 
inte att vara tillgängliga heller. Tar bort allt norm-oljud orsakat av MTGS-trådens eler GPU'ns överdrag. Denna möjlighet användes bäst i förening med sparpunkter: 
spara ett tillstånd i en idealisk scen, möjliggör denna möjlighet, och återladda sparpunkten.


Varning: Denna möjlighet kan möjliggöras dynamiskt men kan vanligtvis inte förhindras på samma vis (video blir ofta skräp). Möjliggör detta ifall Ni tror att MTGS trådsync orsakar krascher eller grafiska fel. Påverkar till vetskap följande spel:
 * Digital Devil Saga (Fixar FMV och crashar)
 * SSX (Fixar dålig grafik och crashar)
 * Resident Evil: Dead Aim (Orsakar förvrängda texturer) Påverkar till vetskap följande spel:
 * Bleach Blade Battler
 * Growlanser II and III
 * Wizardry I första hand inriktat på EE tomgångs-loop hos adress 0x81FC0 i kärnan, denna fix försöker att 
upptäcka loopar vars kroppar garanterat resulterar i samma maskintillstånd för varje upprepning 
tills en schemaläggd händelse utlöser emulering av en annan enhet. Efter en enskild upprepning av en sådan loop, 
avanserar vi till tiden för nästa händelse eller till slutet av processorns tidskvantum, vilket som än kommer först. Att sätta ett högre värde för denna glidare minskar verkningsfullt klockhastigheten hos RörelseMotorns R5900 kärn-CPU, 
och för oftast en hastghetsökning till spel som misslyckas att nyttja möjligheterna med PS2's verkliga hårdvara fullt ut. Denna fix fungerar bäst för spel som använder INTC-statusregistret för att invänta vsyncs, vilket i första hand inkluderar icke-3D 
RPG titlar. Spel som inte använder denna vsync-metod kommer på sin höjd att få en liten uppsnabbning av denna fix. Denna glidare kontrollerar mängden cykler som VU-enheten stjäl ifrån RörelseMotorn. 
Högre värden ökar antalet cykler som stjäls från EE'n för varje VU-microprogram spelt kör. Kolla HDLaddar-kompabilitetslistan för spel som till vetskap kommer till fråga med det här (ofta markerat som behövande 'läge 1' eller 'långsam DVD'). Förmodar att väldigt långt in i framtiden så kommer block inte behöva gammelflagg-instansdata. 
Detta borde vara rätt säkert. Det är okänt ifall detta har sönder något spel... Updaterar endast Statusflaggor för block som kommer att läsa dem, isället för alltid. 
Detta är för det mesta säkert, och Super-VU gör något liknande som standard. Möjliggör automatiskt lägesbyte till fullskärm när emulering statas eller återupptas. 
Ni kan ännu växla mellan helskärm och fönsterläge genom att trycka Alt + Enter. Stänger helt det ofta stora och omfångsrika GS-fönstret 
när Ni trycker på Esc eller avbryter emulatorn. Markera denna för att tvinga muspekaren att bli osynlig när den är i ett GS-fönster; 
användbart om man har musen som främsta kontroll-mojäng för spelande. 
Som förval auto-gömmer sig musen efter 2 sekunders inaktivitet. Möjliggör Vsync när bildfrekvensen är precis på full hastighet. 
Skulle den falla under denna så förhindras Vsync för att undvika vidare prestandastraff. 
Bemärk: Detta fungerar för närvarande bara bra med GSdx som GS insticksprogram, 
och konfigurationen DX10/11 Hårdvarutolkning. 
Annat insticksprogram eller annat tolkningsläge kommer antigen att ignoreras 
eller producera en svart skärm som blinkar när läget byts. 
Det kräver även att Vsync möjliggörs. Vsync slår ut skärmsönderrivning men har oftast en stor prestationseffekt. 
Det tillämpar vanligtvis bara i helskärmsläge, och kanske inte fungerar för alla GS insticksprogram. Förinställningarna tillämpar snabbfixar, några omkompilerarmöjligheter, och några spelfixar som till vetskap höjer hastigheten.
Spelfixar ('Patchar') som till vetskap är viktiga kommer att tillämpas automatiskt.

--> Avmarkera för att modifiera inställningarna manuellt (med nuvarande förinställningar som bas) Förinställningarna tillämpar snabbfixar, några omkompilerarmöjligheter, och några spelfixar som till vetskap höjer hastgheten.
Spelfixar ('Patchar') som till vetskap är viktiga kommer att tillämpas automatiskt.

Förinställningsinfo:
1 -     Den nogrannaste emuleringen, men också den långsammaste.
3 --> Försöker att balansera hastighet med kompabilitet.
4 -     Mera aggresiva fixar.
6 -     För många fixar kommer troligen att sakta ner de flesta spel. Varning! Ni kör PCSX2 med instruktionsmöjligheter som kan överskrida Era konfigurationsinställningar. 
Dessa instruktionsmöjligheter kommer inte att beaktas i Inställningsdialogen, 
och kommer att förhindras om Ni tillämpar förändringar här. Varning! Ni kör PCSX2 med instruktionsmöjligheter som kan överskrida Ert konfigurerade insticksprogram och/eller mappinställningar. 
Dessa instruktionsmöjligheter kommer inte att beaktas i inställningsdialogen, 
och kommer att förhindras när Ni tillämpar förändringar här. 1 - Förvals cykelgrad. Detta överensstämmer nästan med den faktiska hastigheten hos en PS2 RörelseMotor. 2 - Minskar EE's cykelgrad med ungefär 33%. Mild uppsnabbning för de flesta spel med hög kompabilitet. 3 - Minskar EE's cykelgrad med ungefär 50%. Måttfull upsnabbning, 
men *kommer* att orsaka stamningsaudio hos många FMV'er. 1 - Mild VU Cykelstöld. Lägre kompabilitet, men en viss uppsnabbning för de flesta spel. 2 - Måttfull VU Cykelstöld. Ännu lägre kompabilitet, men markant uppsnabbning hos vissa spel. 3 - Maximal VU Cykelstöld. Användningen är begränsad, 
eftersom detta kommer att orsaka synligt flimrande hos de flesta spel. 0 - Förhndrar VU Cykelstöld. Den mest kompatibla inställningen! Förhindrar helt all GS-insticksprogramsaktivitet; 
idealt som utgångspunkt för EE-kärnkomponenter. Enbart för att problemlösa eventuella fel inom MTGS, 
eftersom det möjligen är mycket långsamt. Inkluderar tomgångshändelsebearbetning och några andra ovanliga händelseanvändningar. Det interna minneskorts-insticksprogrammet misslyckades att initialiseras. Loggar händelser medans de skickas till PS2-VM. Ingen anledning given. MTGS tråden har blivit osvarandes under inväntningen av att GS-insticksprogrammet ska öppna. Trådaktivitet: start, avskiljning, sync, borttagning, etc. 