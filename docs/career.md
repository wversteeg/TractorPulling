# Career: the tractor path

The career's classes, from a farm tractor in the standard class to a Super
Stock, with the rules each class sets. Every class on this path is
two-wheel drive: a front-wheel-steered farm tractor pulling on its rear
axle (`tp::vehicle` as it is).

This note keeps two things apart:

- **Rules**: what the NTTO and ETPC rulebooks say, with the source. Data
  files that model a class quote these and say which version they follow.
- **Game choices**: what the game decides for itself (the order of the
  classes, how a player moves up, prices, anything the rules leave open).
  These are marked as such.

## Sources

| Short name | Document | Version | Covers |
|---|---|---|---|
| NTTO 2026 | NTTO wedstrijdreglement ([powerweekend.nl](https://www.powerweekend.nl/wp-content/uploads/Wedtrijdreglement_NTTO_2026.pdf)) | 2026-01 | weights and hitch heights of every class; competition rules |
| NTTO H13 | NTTO reglement, hoofdstuk 13: Farmstocks standaardklasse ([pullingdays.nl](https://www.pullingdays.nl/media/2025-08/NTTO-Reglement-Hoofdstuk-13.pdf)) | 2025 | the standard classes |
| ETPC FS 2026 | ETPC Farmstock Rulebook ([tractorpulling.com](https://tractorpulling.com/wp-content/uploads/2026/02/ETPC-FS-Rulebook-2026.pdf)) | 2026-01, valid to 2029-01-01 | Level 1 Farm, Level 2 Sport, Level 3 Supersport |
| NTTO 2018 | NTTO reglement, all chapters ([pullinggastel.nl](https://pullinggastel.nl/wp-content/uploads/2018/04/NTTO_reglement_2018-04-18.pdf)) | 2018-01 | Superstocks (ch. 3), Light Superstocks (ch. 4), Prostocks (ch. 5), Sport (ch. 15), Supersport (ch. 16) |
| ntto.nl | [www.ntto.nl](https://www.ntto.nl): the 2026 classes, standings and every pull's result | 2026 | which classes are pulled, real distances |

The NTTO does not publish its technical rules on ntto.nl (questions go to
its TVM commission). The Grand National classes' rules (Pro Stock, Limited
Super Stock, Super Stock) come from the **2018** NTTO rulebook, the newest
full one found: check them against the current ETPC main rulebook before
relying on a detail. The weights and hitch heights are the 2026 ones.

## The ladder (game choice)

| # | Class | Section | Weight (2026) |
|---|---|---|---|
| 1 | Standard 3.5 t (standaardklasse) | Farmstock | 3500 kg |
| 2 | Sport 2.6 t (sportklasse) | Farmstock | 2600 kg |
| 3 | Sport 4.5 t | Farmstock | 4500 kg |
| 4 | Supersport 3.4 t (super-sportklasse) | Farmstock | 3400 kg |
| 5 | Supersport 3.6 t | Farmstock | 3600 kg |
| 6 | Supersport 4.5 t | Farmstock | 4500 kg |
| 7 | Pro Stock | Grand National | 3500 kg |
| 8 | Limited Super Stock | Grand National | 3500 kg |
| 9 | Super Stock | Grand National | 3500 kg |

Every Farmstock class the NTTO pulls in 2026 above the standard class, in
order of the engine each allows, then the Grand National classes that
still pull a farm tractor.

The weight goes up and down along the way (3.5, 2.6, 4.5, 3.4, 3.6, 4.5,
then 3.5 t): no one farm tractor ballasts from 2.6 to 4.5 t. So the
player's tractor is the class's tractor as their career has built it: each
class's season names its reference tractor, and the money and upgrades
carry over by name (as step 2 does). Moving up is a rebuild, small within
a weight, large where the weight or the class's engine changes.

A season won opens the next class (`tp::career`, `promotion_place = 1`);
the player chooses to move up or stay.

Not in the game (yet): the NTTO's own levels within a class (see
"Levels" below) and the heavier standard classes.

## The classes (rules)

Weights are the class's maximum, **driver included** (NTTO 2026, art. on
weighing: "Het officiële gewicht van de klasse is inclusief deelnemer").
"Hitch" is the hook's highest point above the ground (NTTO 2026 table,
"Max. trekhaak").

### 1. Standard 3.5 t

Already in the game (`data/classes/ntto_standard_3500.toml`). From NTTO
H13 and NTTO 2026:

- 3500 kg; hitch at most 500 mm ("500 of 550", see the chapter).
- At most 4 cylinders with a turbo.
- At most **85 hp at the PTO** (the other standard weights: 2500 kg 70,
  4500 kg 110, 5500 kg 140, 7000 kg 180, 9000 kg 220, 11000 kg 380).
- Engine speed at most 30 % over the original, to 2700 rpm.
- A safety cab or a roll bar.
- Stock tractor: only safety modifications.

### 2. Sport 2.6 t

The NTTO pulls Sport at 2600 kg (NTTO 2026); the rulebooks write their
lightest Sport limits for **2500 kg** (ETPC 2.1, NTTO 2018 15.B.1). The
limits below are those, to confirm for 2600 kg:

- 2600 kg; hitch as Sport 4.5 t.
- Engine: at most **4 cylinders, turbocharged, 2 valves a cylinder, 4000
  cm³** (NTTO 2018 15.B.1).
- One turbo, its inlet at most **55 mm** or an air restrictor of 55 mm
  (ETPC 2.2.1, 2500 kg). **No intercooler** in the 2500 kg class (ETPC
  2.2.2).
- 2700 rpm (3200 with the Level 3 frame and clutch), diesel, no
  four-wheel drive, as Sport 4.5 t.
- Under 3000 kg: wheelbase at most **2600 mm**, nothing more than **3500
  mm** ahead of the rear axle (ETPC 1.3.1).
- Tires and roll-over protection as Sport 4.5 t (a lighter roll bar: 60 ×
  60 × 4 mm up to 2500 kg, ETPC 2.5.2).

### 3. Sport 4.5 t

ETPC FS 2026, Level 2 (current), and NTTO 2018 ch. 15 (the NTTO's own):

- 4500 kg; hitch at most 500 mm, at least 450 mm behind the rear axle's
  centre, no link above the rear axle's centre line (NTTO 2018 15.F).
- Engine: at most **6 cylinders, turbocharged, 2 valves a cylinder, 7000
  cm³** (NTTO 2018 15.B.1). Bored out or down is allowed. The engine,
  clutch housing, gearbox and rear axle are one make, and the engine came
  in a front-wheel-steered farm tractor.
- **One turbo**, its inlet at most **68 mm** or an air restrictor of 68 mm
  (ETPC 2.2.1, 3500 kg and up).
- Intercooler allowed, with an air restrictor (ETPC 2.2.2; the 2018 NTTO
  chapter still forbade it). No water injection.
- **2700 rpm** (ETPC 2.2.1); up to 3200 rpm with a frame, certified clutch
  and protection and a cable round the engine (Level 3 rules).
- Diesel only. No overhead camshafts unless the engine came with them.
- **No four-wheel drive** (NTTO 2018 15.B.18).
- Wheelbase at most 2900 mm; nothing more than 4000 mm ahead of the rear
  axle's centre line, weights included (NTTO 2018 15.B.19; ETPC 1.3.1
  for tractors over 3000 kg).
- The hood and grille where the maker put them, the sheet metal of stock
  length in its stock place (ETPC 1.3.2, Sport and Supersport).
- Tires: rubber, no duals, **at most 30.5" (800 mm) wide on a 32" rim, or
  up to 710 mm wide on a 42" rim**; cut tires and puller tires allowed
  (ETPC 1.2.5).
- An approved roll-over protection: a factory ROP, a safety cab, an ETPC
  ROP or ETPC roll bar (ETPC 2.5.2). **A cab is no longer needed.**
- Wheelie bars mandatory.

### 4. Supersport 3.4 t

ETPC FS 2026, Level 3:

- 3400 kg; hitch as Sport.
- Engine (ETPC 3.2.1): at most **4 cylinders**, one of **5500 cm³ with 2
  valves a cylinder at 2700 rpm**, **5000 cm³ with 4 valves at 2700 rpm**
  or **4600 cm³ with 4 valves at 3200 rpm**.
- Turbo, intercooler and water injection (in front of the compressor) as
  Supersport 4.5 t; free water injection only at 4.5 t.
- Over 3000 kg: wheelbase at most 2900 mm, 4000 mm ahead (ETPC 1.3.1).
  (The NTTO's 2018 rules held the 3400 kg class to 2600 and 3500 mm.)
- Frame, roll cage, clutch and tires as Supersport 4.5 t.

### 5. Supersport 3.6 t

ETPC FS 2026, Level 3:

- 3600 kg; hitch as Sport.
- Engine (ETPC 3.2.1): at most **6 cylinders**, **7000 cm³ at 2700 rpm**
  or **6063 cm³ at 3200 rpm**, 4 valves a cylinder.
- Everything else as Supersport 3.4 t.

### 6. Supersport 4.5 t

ETPC FS 2026, Level 3, and NTTO 2018 ch. 16:

- 4500 kg; hitch as Sport.
- Engine (ETPC 3.2.1): at most **8 cylinders**, 4 valves a cylinder, and
  either **8364 cm³ at 3200 rpm** or **9000 cm³ at 2700 rpm**; any amount
  of downsizing.
- **One turbo, one stage** (ETPC 1.2: one pressure stage, no blowers).
  Limits (ETPC 3.2.2): compressor inducer at most **121.9 mm (4.8")**,
  housing inlet 124.4 mm; turbine wheel at most 127 mm (5").
- Intercooler allowed; water injection in front of the compressor, and
  free in the 4500 kg class.
- Fuel pump free (at most 2 elements a cylinder, NTTO 2018 16.B.16);
  diesel only.
- No four-wheel drive (NTTO 2018 16.B.18). Wheelbase at most 2900 mm.
- Clutch, flywheel and protection to the ETPC Pro Stock specification. A
  safety frame from the front of the tractor to the rear axle. A **roll
  cage** (NTTO 2018 16.C.14).
- Tires as Sport. Component chassis allowed in Level 3 (ETPC).
- The OEM clutch housing, gearbox and rear axle housings; the look of the
  farm tractor stays.

### 7. Pro Stock

NTTO 2018 ch. 5 (the Grand National rules; check against the current
ETPC main rulebook):

- **3500 kg**; hitch at most 500 mm (NTTO 2026).
- Engine: at most **8364 cm³ (510 ci)**, **7374 cm³ (450 ci)** with a
  multi-valve head; an OEM farm-type head on the original block of the
  same make.
- **One turbo, one stage**: compressor wheel at most **127 mm (5")**,
  housing inlet 128.8 mm; turbine wheel at most 132.1 mm (5.2").
- Intercooler and water injection allowed.
- **Diesel only**; alcohol forbidden. One fuel pump, size free.
- Only tractors sold as front-wheel-steered farm tractors. The OEM clutch
  housing, gearbox, rear axle and final drives (planetaries stay).
  Component chassis allowed within the ETPC, with conditions.
- **The hood and grille where the maker put them; the tractor's stock
  look stays.** A newer model's sheet metal is allowed with permission.
- Wheelbase at most 2900 mm (or the original, if longer); nothing more
  than 4000 mm ahead of the rear axle.
- A safety frame; the hitch and roll cage part of the frame.
- The NTTO pulls Pro Stocks in two groups (I and II) with promotion and
  relegation (see "Levels").

### 8. Limited Super Stock

NTTO 2018 ch. 4 ("light superstocks"; the 2026 class is called Limited
Superstocks):

- **3500 kg**; hitch at most 500 mm (NTTO 2026).
- Engine: **diesel 8364 cm³ (510 ci)**, or **methanol 7600 cm³ (466 ci)**
  with 2 valves a cylinder or **7000 cm³ (427 ci)** with 4.
- **One turbo, one stage**, the compressor housing's inlet at most
  **110 mm**. Intercooler allowed.
- Hood and grille where the maker put them; frame, hitch and roll cage as
  Pro Stock.

### 9. Super Stock

NTTO 2018 ch. 3:

- **3500 kg**; hitch at most 500 mm (NTTO 2026).
- Engine: at most **10650 cm³ (650 ci)**; a diesel may be converted to two
  stroke.
- **Up to three stages of boost with at most four turbos.**
- Diesel or alcohol (the rules set the shielding for alcohol tractors).
- **The hood and grille of the original length** where the maker put
  them: still a recognisable tractor.
- Frame, hitch and roll cage as Pro Stock.

### Tires in the Grand National classes

NTTO 2018 ch. 2 lists, for the classes it covers, at most 30.5" or
800/65 wide, and rims up to 38" (650/75R32, 620/75R34, 580/70R38). Which of
these apply to Pro Stock and the Super Stocks needs the current ETPC main
rulebook.

## Levels within a class (rules)

The NTTO splits a class into levels, each its own competition:

- Farmstock classes in 2026: **topniveau**, **promoniveau** and
  **beloftenniveau** (ntto.nl, 2026 standings: Sport 4.5 t and
  Supersport 3.6 t have all three; Sport 2.6 t, Supersport 3.4 and 4.5 t
  top and promo).
- Pro Stocks (NTTO 2018 art. 1.16): **group I and group II**. A new
  machine starts in group II. The **winner of group II moves up** to group
  I and **12th of group I moves down**. Nobody moves up or down by choice,
  and group I is filled up to 12 from group II.

A game choice for later: a class's seasons could start at the lowest
level, with the same promotion as Pro Stock groups.

## Real distances (ntto.nl)

For tuning the classes' sleds and rivals, from the 2026 results:

- Supersport 4.5 t, top level: full pulls in the first run, pull-offs to
  about **105 to 122 m**.
- Pro Stock: pulls of about **80 to 100 m**, with full pulls at some
  events.

## What the simulation needs per class (game)

The rules above become, for each class: a class rules file
(`data/classes/`), a reference tractor and its engine, tires and sled, a
garage with the class's upgrades, and a season. Beyond what the core has:

- Class rules today check mass, PTO power, engine speed and hitch height
  (`rules::ClassRules`). Sport and up limit the **engine's build** instead
  of its power: cylinders, displacement, valves, turbos and their size,
  fuel. These need to become class limits too.
- No class above Standard limits power directly. Each class's engine data
  needs a believable torque curve for its limits (a 7 l Sport engine at
  2700 rpm, a 10.6 l three-stage Super Stock), estimated, not from the
  rules.
- Tire width and rim size are class limits from Sport on, so tires need
  their real size as a part (see `future_improvements.md`).
- The faster classes need sled drag that grows with speed.
