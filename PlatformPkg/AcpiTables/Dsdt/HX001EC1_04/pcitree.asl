

    Name(PR00, Package(14)
    {
        Package(4) {0x0002FFFF, Zero, LNKH, Zero}, 
        Package(4) {0x0004FFFF, Zero, LNKH, Zero},          
        Package(4) {0x0005FFFF, Zero, LNKH, Zero}, 
        Package(4) {0x0001FFFF, Zero, LNKH, Zero}, 
        Package(4) {0x0001FFFF, One,  LNKH, Zero},
        Package(4) {0x0008FFFF, Zero, LNKH, Zero}, 
        Package(4) {0x0009FFFF, Zero, LNKH, Zero}, 
        Package(4) {0x000AFFFF, Zero, LNKA, Zero}, 
        Package(4) {0x000AFFFF, One,  LNKB, Zero}, 
        Package(4) {0x000AFFFF, 0x02, LNKC, Zero}, 
        Package(4) {0x000AFFFF, 0x03, LNKD, Zero}, 
        Package(4) {0x000BFFFF, Zero, LNKA, Zero}, 
        Package(4) {0x000CFFFF, Zero, LNKA, Zero}, 
        Package(4) {0x0014FFFF, Zero, LNKA, Zero}        
    })
        
    Name(AR00, Package(14)
    {
        Package(4) {0x0002FFFF, Zero, Zero, 0x23}, 
        Package(4) {0x0004FFFF, Zero, Zero, 0x1B},         
        Package(4) {0x0005FFFF, Zero, Zero, 0x18},
        Package(4) {0x0001FFFF, Zero, Zero, 0x29}, 
        Package(4) {0x0001FFFF, One,  Zero, 0x2A}, 
        Package(4) {0x0008FFFF, Zero, Zero, 0x28},
        Package(4) {0x0009FFFF, Zero, Zero, 0x24}, 
        Package(4) {0x000AFFFF, Zero, Zero, 0x10}, 
        Package(4) {0x000AFFFF, One,  Zero, 0x11}, 
        Package(4) {0x000AFFFF, 0x02, Zero, 0x12}, 
        Package(4) {0x000AFFFF, 0x03, Zero, 0x13}, 
        Package(4) {0x000BFFFF, Zero, Zero, 0x15}, 
        Package(4) {0x000CFFFF, Zero, Zero, 0x16}, 
        Package(4) {0x0014FFFF, Zero, Zero, 0x11}
    })

    Name(PR01, Package(4)
    {
        Package(4) {0xFFFF, Zero, LNKH, Zero}, 
        Package(4) {0xFFFF, One, LNKH, Zero}, 
        Package(4) {0xFFFF, 0x02, LNKH, Zero}, 
        Package(4) {0xFFFF, 0x03, LNKH, Zero}
    })

    Alias (PR01, PR06)    
    Alias (PR01, PR08)

    //PEGS APIC Routing
    Name(AR01, Package(4)
    {
        Package(4) {0xFFFF, Zero, Zero, 0x20}, 
        Package(4) {0xFFFF, One, Zero, 0x21}, 
        Package(4) {0xFFFF, 0x02, Zero, 0x22}, 
        Package(4) {0xFFFF, 0x03, Zero, 0x23}
    })                 

    //PG0S APIC Routing
    Name(AR06, Package(4)
    {
        Package(4) {0xFFFF, Zero, Zero, 0x18}, 
        Package(4) {0xFFFF, One, Zero, 0x19}, 
        Package(4) {0xFFFF, 0x02, Zero, 0x1A}, 
        Package(4) {0xFFFF, 0x03, Zero, 0x1B}
    })      

    //PG2S APIC Routing
    Name(AR08, Package(4)
    {
        Package(4) {0xFFFF, Zero, Zero, 0x19}, 
        Package(4) {0xFFFF, One, Zero, 0x1A}, 
        Package(4) {0xFFFF, 0x02, Zero, 0x1B}, 
        Package(4) {0xFFFF, 0x03, Zero, 0x18}
    })
		      

    Name (PRSA, ResourceTemplate()
    {
        IRQ (Level, ActiveLow, Shared,) {3,4,5,6,7,10,11,12,14,15}
    })

    Alias (PRSA, PRSB)
    Alias (PRSA, PRSC)
    Alias (PRSA, PRSD)
    Alias (PRSA, PRSE)
    Alias (PRSA, PRSF)
    Alias (PRSA, PRSG)
    Alias (PRSA, PRSH)
           