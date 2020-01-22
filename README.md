# Fallblattanzeiger der Deutschen Bahn

Die Deutsche Bahn verwendete lange Zeit standardisierte Fallblattanzeigemodule mit Platinen, die mit *MANTronic* beschriftet sind, also vermutlich von der Firma MAN hergestellt wurden.

Möchte man diese alten Module wieder zum klappern bringen, so hat man grundsätzlich erstmal zwei Möglichkeiten. Die Module wurden offenbar über eine RS485-Schnittstelle angesteuert. Das Protokoll ist vermutlich ein Protokoll ähnlich dem CAN-Bus. Meine gründliche Recherche ergab allerdings keine Quellen im Internet, denen bekannt ist, wie dieses Protokoll genau aussieht. Diese Möglichkeit ist daher eher schwierig.

Eine andere Möglichkeit besteht darin, den zentralen ZiLOG-Microcontroller auf der Platine durch einen eigenen Microcontroller zu ersetzen. Ich habe zu diesem Zweck den ZiLOG-Chip von der Platine geschnitten. Praktischerweise sind auf der Platine nicht nur die SMD-Lötpads für den ZiLOG-Chip, sondern auch etwas größere Pads im DIP-Raster, an die man sehr gut Kabel anlöten kann. Vermutlich ein überbleibsel von älteren Platinenversionen, bei denen ein größerer Microcontroller verwendet wurde.

Mit einer Kombination aus eigener Intuition und Bestätigung durch die Schaltpläne dieses [Hackaday-Projekts](https://hackaday.io/project/9070-fallblattfahrzielanzeige-of-s-bahn-berlin) war es nicht schwer, die Pinbelegung des ZiLOG-Controllers zu ermitteln und so herauszufinden, welche Pins angesteuert bzw. ausgelesen werden müssen. Weiter unten habe ich das mit einer Skizze und einem Foto dokumentiert.

Gemäß der Einteilung im [Wikipedia-Artikel](https://de.wikipedia.org/wiki/Fallblattanzeige) in die vom Aufbau her zu unterscheidenden Epochen 1 bis 4 fällt diese Anzeige in die Epoche 3.

## Funktionsweise

Die Funktionsweise des Fallblattmoduls ist gar nicht so kompliziert. Die Blätter sitzen auf einer Welle die über ein Getriebe von einem Synchronmotor (Berger Lahr RSM 42/12 NG) angetrieben wird, der tatsächlich auch heute noch erhältlich ist ([Datenblatt](https://www.abi.nl/assets/uploads/Downloads/Aandrijvingen/Kleine%20vertragingsmotoren/Synchroonmotoren/Catalogus_RSM_D.pdf)).
Damit der Motor weiß, wann er anhalten soll, sind zwei Infrarot-Transmitter mit Photodioden als Sensoren verbaut. Ein Sensor erzeugt pro fallendem Blatt einen erhöhten Spannungspegel, der andere dient zur Erkennung der Nullposition und weist nur einmal pro Umdrehung der ganzen Welle einen erhöhten Spannungspegel auf. Das ganze wird durch weiße Blenden bewerkstelligt, die beim Vorbeistreichen am Sensor durch die Reflektion der Infrarotstrahlen des Transmitters für mehr Einfall von Infrarotstrahlung in die Photodiode und dadurch einen höheren Spannungspegel sorgen.

Die Verdrahtung der Sensoren auf der Platine ist etwas eigenwillig. Beide Sensoren gehen auf den gleichen Eingang (Sensor IN), an dem mit Hilfe eines ADC die anliegende Spannung messen kann. Um die Sensoren einzeln abfragen zu können, kann man die Sensoren getrennt mit Spannung versorgen (Sensor 1 / Sensor 2) und dann jeweils nur den Sensor auslesen, der mit Spannung versorgt wird. Der Motor ist einfach anzusteuern: Zieht man den Motor-Pin auf LOW, läuft der Motor los, zieht man ihn auf HIGH (3,3V oder 5V), bleibt der Motor stehen.

## Pinbelegung ZiLOG
![Pinbelegung](/images/zilog.png)
![Pinbelegung](/images/schaltskizze.png)

## Stromversorgung

Die Platine hat zwei Stromkreise, ein Niederspannungsstromkreis für die Steuerungselektronik, der ursprünglich wohl mit 5 V DC betrieben wurde. Ich habe auch mit nur 3.3 V DC keine Probleme festgestellt. Der andere Stromkreis sollte mit etwa 42 V AC bei 50 Hz gespeist werden. Mit diesem Strom wird der Synchronmotor zum Antrieb der Welle betrieben. Die folgende Skizze zeigt die Pinbelegung des Wannensteckers am Rand der Platine bezüglich der Spannungsversorgung.

![Stromversorgung](/images/power.png)

## Code

Code für die Platine kann in den Repositories [fallblatt-code-legacy](https://github.com/julianschick/fallblatt-code-legacy) und [fallblatt-code](https://github.com/julianschick/fallblatt-code) gefunden werden, wobei letzterer aktueller und mit mehr Funktionen ausgestattet ist.

## Teileliste

Mit passenden Artikelbezeichnungen für den Berliner Elektroteilehändler [Segor](https://www.segor.de).

Anzahl | Beschreibung | Segor-Artikelbezeichnung | Alt. Link
--|--|--|--
1 | Schraubklemme 2-polig RM 7,5mm | *ARK 2-Lift/RM7,5*
1 | Varistor >300V RM 7,5mm | *VDR 300-K 7* | [Reichelt](https://www.reichelt.de/varistor-rm-7-5-mm-300-v-10-epc-b72210-s030-p239932.html)
1 | Brückengleichrichter Rund, Wechselstrompins gegenüber, RM 5mm | *B 80C1500 R* | [Reichelt](https://www.reichelt.de/brueckengleichrichter-100-v-1-5-a-b70c1500rund-p181713.html)
1 | Transformator 9V _Block VB 2,0/1/9_ | | [Reichelt](https://www.reichelt.de/printtrafo-2-va-9-v-222-ma-rm-20-mm-ei-30-15-5-109-p27328.html)
1 | Transformator 2x24V *Block VC 10/2/24* | | [Reichelt](https://www.reichelt.de/printtrafo-10-va-2x-24-v-2x-208-ma-rm-27-5-mm-ei-48-16-8-224-p27492.html)
1 | ESP32-Development-Board (es gibt 2 Varianten bei eBay, für diese Platine wird die unüblichere mit 19 Pins je Reihe und GND/VCC auf gegenüberliegenden, nicht nebeneinanderliegenden Pins benötigt; diese wird oft unter dem Namen _ESP32S_ verkauft.) |
1 | Spannungsregler 3,3V Formfaktor TO-220 _STM LD1117 V33C_ | *LD 1117 V33* | [Reichelt](https://www.reichelt.de/ldo-regler-fest-3-3-v-to-220-ld1117-v33c-p200891.html)
1 | Elektrolytkondensator 470u, >12V, RM 5mm | *ELRA 470u-25/105°lowESR* |
1 | Elektrolytkondensator 10u, >12V, RM 2mm | *ELRA 470u-35/105°* |
1 | Kondensator 100n, >12V, RM 2,5mm | *u10-R2.5-X7R* |
1 | D-Sub Buchse 9-polig, Printmontage 90°, Abstand Pinreihe zu Steckerfläche 10mm | DS09F-90°-10mm |
1 | D-Sub Buchse 25-polig, Printmontage 90°, Abstand Pinreihe zu Steckerfläche 10mm | DS25F-90°-10mm |
4 | Pinheader 20-polig, Female, Vertikal, RM 2,54mm | 
2 | Pinheader 2-polig, Male, Vertikal, RM 2,54mm
