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
| ETPC 2026 | ETPC Rulebook 2026 ([tractorpulling.com](https://tractorpulling.com/wp-content/uploads/2026/07/ETPC-Rulebook-2026.pdf)); limits set for 2026–2028 | 2026 | weights, hitches, tires and engines of Pro Stock, Limited Super Stock, Super Stock |
| ETPC letters | Pro Stock turbo clarification ([22 June 2026](https://tractorpulling.com/wp-content/uploads/2026/06/ETPC-TS-letter-Pro-Stock-Turbo-clarification-Final-update.pdf)), Limited Super Stock restrictor ([15 January 2026](https://tractorpulling.com/wp-content/uploads/2026/01/ETPC-information-letter-restrictors-LSS-.pdf)), [changes for 2026](https://tractorpulling.com/wp-content/uploads/2026/02/ETPC-Rulebook-changes-for-2026.pdf) | 2026 | details of the above |
| NTTO 2018 | NTTO reglement, all chapters ([pullinggastel.nl](https://pullinggastel.nl/wp-content/uploads/2018/04/NTTO_reglement_2018-04-18.pdf)) | 2018-01 | Superstocks (ch. 3), Light Superstocks (ch. 4), Prostocks (ch. 5), Sport (ch. 15), Supersport (ch. 16) |
| ntto.nl | [www.ntto.nl](https://www.ntto.nl): the 2026 classes, standings and every pull's result | 2026 | which classes are pulled, real distances |

The NTTO does not publish its technical rules on ntto.nl (questions go to
its TVM commission). The Grand National classes follow the ETPC Rulebook
2026; the Farmstock classes the ETPC Farmstock Rulebook 2026 and, for
what it leaves to the NTTO, the 2018 NTTO rulebook (the newest full one
found). The ETPC Rulebook 2026 is a scanned PDF, read by OCR: its figures
here were checked by eye.

## The ladder (game choice)

| # | Class | Section | Weight (2026) |
|---|---|---|---|
| 1 | Standard 3.5 t (standaardklasse) | Farmstock | 3500 kg |
| 2 | Sport 2.5 t (sportklasse) | Farmstock | 2500 kg (the NTTO pulls 2600 kg) |
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

**Each class's tractor is bought from its dealer (game choice).** A new
career starts at the Standard 3.5 t dealer, and a class the player has not
pulled in yet at its own: the season waits for the tractor. (The parts
carried into the class by name, as every move up does, come along on the
new tractor: the dealer sells the tractor, not its parts.) The dealer
sells five models of the class's tractor and eight colours
(`[garage.dealer]` in each class's garage file); for now the models only
look different (`TractorModels` in the game), each pulling the same, so the
choice is the player's taste: a later version may give them their own
engines or weights. The Standard 3.5 t's are farm tractors, the Sport
classes' pulling tractors under a modern pulling bonnet, and the
Supersport 3.6 t's farm tractors rebuilt for pulling, as the class's real
ones are (videos and the ETPC results: an IHC 1066, a Ford 8970): each
keeps its maker's hood and grille, as the rules ask, on a pulling frame,
no cab but a full roll cage round the seat (the class asks one: a narrow
cell of arched hoops over the driver's helmet), one big stack off the
single turbo (one turbo, one stage) and a weight rack or a big painted
weight box ahead of the grille. A driver on a pulling frame, Sport or
Supersport, wears a helmet. The rear tires are drawn as fitted: the
modified farm tires' tall lugs or the puller tires' many low bars. The prices are a part of the money a class's player
has: the Standard 3.5 t's models cost € 1,500 to 2,500, with € 2,500 to
start, so the cheapest leaves the € 1,000 the garage was balanced for (a
dearer one, less: the first upgrades come a little later); the Sport
2.5 t's € 4,000 to 6,500, the Sport 4.5 t's € 6,500 to 9,800 and the
Supersport 3.6 t's € 8,500 to 12,300, a fifth
to a third of what a winning season at the level below pays, so a player
who moves up with a fully built tractor and some money left buys one
outright, and one who spent everything still gets the cheapest
(`garage::model_price_eur`: never dearer than the money there is). The
garage's paint shop paints the tractor another colour between events
(€ 300, 500 and 700). The rivals drive the dealer's other models.

**Every class has its own tractor (game choice).** The weight goes up and
down along the way (3.5, 2.5, 4.5, 3.4, 3.6, 4.5, then 3.5 t), and each
class allows a different engine, so no one tractor climbs the whole
ladder. Moving up a class means a new tractor for that class, built from
its base (each class's season names it), and **parts of the old one can
move across** where they fit and the new class allows them:

- **Wheels and tires**: the same rims and tires, while within the new
  class's limits: 30.5" / 800 mm on 32" (710 mm on 42") from Sport to
  Supersport and in Super Stock, but at most 650/75R32 (635 mm of tread)
  in Pro Stock and Limited Super Stock.
- **The rolling frame and safety parts**: the safety frame, roll bar or
  cage, wheelie bars, the hitch, the clutch protection (the Sport,
  Supersport and Pro Stock frame rules are much alike).
- **Engine parts**: the turbo, fuel pump, intercooler, while the new
  class's limits allow them (a 4.8" turbo is legal in Supersport, not in
  Sport's 68 mm inlet).
- **Free weight**: a tractor is at its class's weight, its weights fixed at
  the back; the garage frees them, level by level, to be moved forward
  before a pull (`chassis::with_free_weight`).

What cannot move is what makes the class: the engine block and its
displacement, the base tractor's chassis. The garage then shows each part
as fitting the tractor, legal in the class, or legal but a poor choice.
Money carries over as it is.

This is step 3 (real parts) of the plan. Until then, step 2 carries the
garage's upgrade levels by name from one class's garage to the next.

Each class is pulled at its levels, belofte, promo and top; the standard
class, the warming-up, at one level, and the Sport 2.5 t introduction
class at promo and top (see "Levels within a class"): a season finished
first or second opens the next level at once, and a class's last level
the next class (`tp::career`, `promotion_place = 2`, `min_seasons = 1`);
the player chooses to move up or stay.

Not in the game: the heavier standard classes.

## The classes (rules)

Weights are the class's maximum, **driver included** (NTTO 2026, art. on
weighing: "Het officiële gewicht van de klasse is inclusief deelnemer").
"Hitch" is the hook's highest point above the ground (NTTO 2026 table,
"Max. trekhaak").

### 1. Standard 3.5 t

Already in the game (`data/classes/ntto_standard_3500.toml`), the
career's warming-up for the controls, at one level (see "Levels within a
class"). From NTTO H13 and NTTO 2026:

- 3500 kg; hitch at most 500 mm ("500 of 550", see the chapter).
- At most 4 cylinders with a turbo.
- At most **85 hp at the PTO** (the other standard weights: 2500 kg 70,
  4500 kg 110, 5500 kg 140, 7000 kg 180, 9000 kg 220, 11000 kg 380).
- Engine speed at most 30 % over the original, to 2700 rpm.
- A safety cab or a roll bar.
- Stock tractor: only safety modifications.

### 2. Sport 2.5 t

The rulebooks write their lightest Sport class for **2500 kg** (ETPC 2.1,
NTTO 2018 15.B.1); the NTTO pulls it at 2600 kg in 2026 (NTTO 2026), with
no published rules of its own. The game pulls it at 2500 kg, by the
rulebooks (game choice):

- 2500 kg; hitch as Sport 4.5 t.
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

In the game (`data/classes/ntto_sport_2500.toml`,
`data/tractors/sport_2500.toml`): 2500 kg, the hitch at most 0.5 m, 2700
rpm unloaded, and no PTO power limit (the core checks only mass, power,
speed and hitch, so the engine's build limits above are not checked yet).
The tractor is smaller than the standard class's (2.2 m wheelbase, 0.78 m
rear tires) and pulls under a roll bar instead of a cab. The rules set no
power; its engine is set from videos of the class's top tractors (a full
pull in about 20 s from moving off on an ordinary track, the front up on
the launch and again at the end): **about 350 hp** (1050 N·m) fully
built, from a stock farm engine's 145 hp. Eight gears with a
powershift; it pulls in tall, close gears (see "How the Sport classes
pull").

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
  up to 710 mm wide on a 42" rim**. The ETPC allows cut tires and puller
  tires from Sport on (ETPC FS 2026 1.2.5, Level 2 and 3; Level 1 Farm
  neither, 1.4.1). Dutch rulebooks allow cut tires in the sport classes
  but not puller tires (the NTTO's own text and local rulebooks: no
  "pullerbanden"). **The game follows the NTTO**: the Sport classes pull on
  farm tires cut for pulling, and puller tires come from Supersport on.
- An approved roll-over protection: a factory ROP, a safety cab, an ETPC
  ROP or ETPC roll bar (ETPC 2.5.2). **A cab is no longer needed.**
- Wheelie bars mandatory.

In the game (`data/classes/ntto_sport_4500.toml`,
`data/tractors/sport_4500.toml`): 4500 kg, the hitch at most 0.5 m, 2700
rpm unloaded, no PTO power limit. A big six-cylinder tractor (2.75 m
wheelbase, 30.5L-32 cut farm tires at the class's widest) under a roll bar.
Its engine is set from videos of the class's top tractors (about 115 m
in a pull-off in about 15 s): **about 780 hp** (2410 N·m) fully built,
from a stock farm engine's 270 hp. Eight gears with a
powershift, pulled as Sport 2.5 t. All three levels, belofte, promo and
top.

#### The setup class (game choice)

Each class adds something to learn: the standard class the controls,
Sport 2.5 t a careful clutch and where the weights go, Sport 4.5 t the
**setup for the track**. The stronger and heavier the tractor, the more
the track decides: the light standard tractor runs out of engine before
grip on most tracks, a Sport 4.5 t does not (`TrackSetup.*`):

- **Grippy tracks** (the firm loam, the dry hard clay): the chain, hooked
  low on the sled's pan, pulls the hitch down and the nose up, and the
  tires have grip to spare: the weights go forward to hold the nose down.
  With the weights back the tractor stands on its wheelie bar, and the
  bar takes load the rear tires lose. Soft tires' squirming lugs cost
  metres, and a hitch a hole or two below the top may pull further.
- **The heavy clay**: the tires dig in and bite, the sled ploughs, and the
  engine is pulled down to a stall before the tires spin out. The weights
  go forward, and soft tires pay.
- **The loose sandy clay**: the grip runs out first, so the weights stay
  a quarter forward, the most load on the rear tires the front allows.
- **The watered start**: wet only at the start, grippy after it: the
  weights about halfway, enough on the rear tires to get the sled going,
  not so much that the grippy rest lifts the nose.
- **Less grippy tires** want the weights further back than the puller
  tires on the same track.
- **The hitch, set per pull**: a pulling hitch with holes from 0.40 m
  (the tractor's `hitch_min_height_m`) to the top hole the garage's
  drawbar gives, at most the class's 0.50 m, set on the setup screen
  beside the weights and the tire pressure (in every class; with the farm
  drawbar alone it has one hole, until a higher one is bought). Higher
  puts more of the sled's pull on the rear tires but lifts the front, and
  with it the steering, sooner.
- **The turbo**: the stock tractor comes with a small, quick turbo
  (`engines/sport_4500_fast_turbo.toml`, about 660 hp fully built); the
  68 mm turbo (`engines/sport_4500.toml`, the rivals' too, about 780 hp)
  is the upgrade: over the season clearly further on most tracks and
  never clearly behind
  (`TurboChoice.TheSixtyEightMillimetreTurboIsTheUpgrade`). It is a
  garage choice (owned side by side, one fitted), the garage the
  Supersport classes' engine, tires and gearbox choices will use: there
  the alternatives are each best somewhere (an engine at 2700 rpm or one
  at 3200 rpm; traction or wheelspin).

The rivals keep one build each (the 68 mm turbo, the weights and hitch
where the field puts them); rivals setting up for the track are in
`docs/future_improvements.md`.

#### How the Sport classes pull (game)

The Sport engines can drive their wheels far harder than farm tires
grip, so at first every Sport pull ended in wheelspin with the engine on
its governor and a powershift never fired. Real Sport pulls end either
way, depending on the gear and the track: too tall a gear bogs the engine
(and on a grippy track stalls it), too low a gear spins out, and the
powershift pulls a slightly too tall gear through. The game gets there
with three things:

- **Cut farm tires** (`data/tires/sport_*rear.toml`) grip harder than
  uncut ones, about 1.5 times, and build their grip with more slip (a
  third of an uncut tire's rate), so they still spin up at the launch and
  the engine keeps its revs. Cut lower and sharper, their lugs do not dig
  in and smear when spun fast as an uncut farm tire's do. (Until the tire
  model knew how fast a tread spins, these were puller tires, which the
  NTTO's Sport classes do not allow; their grip is as it was.)
- **Puller tires and farm tires differ in how they spin**
  (`tire::TractionCurve`): a farm tire's few tall lugs each push against
  a block of soil, so it grips at little slip, and spun fast it digs in and
  smears (grip falls to `fast_spin_grip`); a puller tire's many low, sharp
  bars each cut a thin slice of fresh track and throw it out, so it bites
  harder the faster its tread sweeps (from `slow_spin_grip` up to full
  grip over a slip speed of about `spin_speed_m_s`) and wants gearing tall
  enough to spin it up; the side grip follows the same share. The
  standard class's farm tire smears a little (5 %); the Sport classes'
  cut ones do not; the puller tire comes in Supersport 3.6 t, and with it
  the tall final drive it wants.
- **Tall, close pulling gears**, 5th to 8th about 15 % apart, the wheels
  turning at about 27 to 42 km/h at 2600 rpm: speed and momentum count.
  1st to 5th are about 40 % apart (4th about 20 km/h), so 3rd and 4th
  are the low gears for a wet start or a weak, new engine.
- **A held launch**: high revs, the clutch let in over about 3 s but eased
  back while the engine is below 80 % of its rated speed, for up to 8 s
  (`driver::Plan::launch_hold_share`, the events' `[event.driver]`). The
  game's easy clutch assist works the clutch much the same way: it slips
  the clutch while the engine is below 60 % of its rated speed.

The sleds are heavier to match (the weight box: 1800 kg for Sport 2.5 t,
4500 kg for Sport 4.5 t). The grip is an estimate, set so that the
behaviour above appears; real measurements would set it better.

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

In the game (`data/classes/ntto_supersport_3600.toml`,
`data/tractors/supersport_3600.toml`): 3600 kg, the hitch at most 0.5 m,
3200 rpm unloaded (the class's faster engine's limit; the 7 l engine's
own governor cuts at 2700), no PTO power limit. The class comes after
Sport 4.5 t in the career (Supersport 3.4 t is not in the game yet), at
all three levels. Fully built on the stock 3 bar turbo, both engines
give about **1000 hp**; on the garage's biggest, 7 bar, the 6.06 l gives
about **1320 hp**, as the top tractors make, and the 7.0 l about 1200 hp.
The rivals run bigger turbos too at promo (3 to 3.5 bar) and top (3 to
4.5 bar; `boost_bar` in the event's field). The
NTTO's top tractors pull about 105 to 115 m with much wheelspin, the front
low (videos of 2026 pull-offs; "the distance to beat 114.89 m").

#### Engines, tires and gearing (game choice)

Supersport 3.6 t is the class of **choices**: three parts the garage sells
side by side, each fitted between events (K-6), each best somewhere:

- **The engine**: the stock **7.0 l six at 2700 rpm**
  (`engines/supersport_3600_7l.toml`: torque low down, a heavy flywheel)
  or the **6.06 l six at 3200 rpm** (`engines/supersport_3600_6l.toml`,
  € 6,000: less torque, more revs, a lighter flywheel). Each has its own
  build (eleven and eight stages) and governor upgrade; a build counts
  only while its engine is fitted.
- **The rear tires**: the stock **modified farm tires** (710/70R42, its
  tall lugs regrooved: grip at little slip, smearing when spun fast) or
  **puller tires** (30.5L-32, € 5,000: many low bars that bite weakly
  turning slowly, fully at moderate spin and keep most of it spun fast,
  and a higher limit). Each has its own grip upgrade, from worn to hand
  cut (farm 0.68 to 0.80, puller 0.80 to 1.00 of the load at full bite).
  Fully built, the puller tires on the final drive that suits the track
  pull further than the farm tires on the short one on every Supersport
  track (`TireChoice.FarmTiresLoseToPullerTiresOnEveryTrack`): the farm
  tires are the cheap ones to start the class on. Both grip about as real
  tires do (the puller tire about its load at full bite, the farm tire a
  little less), so the engine has power to spare and spins the puller
  tires fast. A top pull, checked against NTTO figures and F = ma: geared
  for wheel speed (6th on the middle drive), on a firm track the wheels
  spin at 65 to 80 km/h through the middle of the track while the tractor
  goes at 30 to 37 km/h over the ground, and it is at the 100 m line about
  13 s after the sled first moved; the farm tires turn at 25 to 45 km/h
  (`TireChoice.ATopPullSpinsTheWheelsFastAndTakesThirteenSeconds`).
  A tire of another size moves the axle and the tractor's centre of
  gravity with it (`vehicle::with_rear_tire`).
- **The final drive**: the stock **short** one
  (`drivelines/supersport_3600_short.toml`, 30:1), the **middle** one
  (`_medium.toml`, 14.5:1, € 3,000) or the **tall** one (`_tall.toml`,
  10.5:1, € 5,000). Each says which tires it suits (`suits` in the garage
  file): the short one keeps the farm tire turning slowly, gripping
  without smearing; the middle and tall ones spin the puller tires up,
  the tall one fastest. Teams gear for wheel speed (85 to 100 km/h at the
  wheel), and the rivals pull in 5th and 6th on the middle drive; a lower
  gear may still pull a few metres further (docs/future_improvements.md).
  The crew chief says which final drive
  the fitted tires want, and to buy it first if it is not owned yet
  (`garage::advice`, on the garage screen); the player still chooses.
- **The turbo** (an upgrade for either engine, `boost_bar`): the stock
  3 bar turbo, then 4, 5, 6 and 7 bar (€ 5,000, 9,000, 14,000 and
  20,000: the dearest part, about three seasons at the top to buy). Each
  bar more gives more torque at full boost (`engine::with_boost_pressure`;
  7.5 % of the 6.06 l's own a bar, 4.5 % of the 7.0 l's, as
  `[engine.turbo.bigger]` in each engine's file), the torque without
  boost the same, and the boost later (125 rpm a bar) and slower (0.12 s
  a bar). The garage's dyno shows the curve with the boost settled
  (`engine::settled_net_torque_nm`): a big turbo is weak low down. The
  power counts: on the firm loam the 6.06 l pulls clearly further on 7 bar
  than on 3 bar
  (`TireChoice.TheBiggestTurboPullsFurthestAndMostOnTheSixLitre`).
- **A pulling gearbox**: two gears to manoeuvre and four close pulling
  gears (3rd to 6th, about 13 % apart); no powershift, one gear the whole
  pull.
- **The start** (every class): the sled breaks away harder than it rolls
  (9 % of its weight at rest, `sled::Params::breakaway_share`), so the
  chain holds the tractor a moment while the clutch slips and the turbo
  spools; the scripted pullers feather the held clutch
  (`launch_feather_share`), fully in within the hold's longest time (about
  30 m at the latest), and pull each gear at full or eased (85 %) hand
  throttle, whichever went further at their gear trial.

#### How a Supersport pulls (game)

- **"Optoeren"**, building boost before the launch: real pullers hold
  the brakes and let the clutch bite so the load spools the turbo. A
  Supersport's clutch (60 to 100 kg on the pedal) leaves no foot for the
  brakes, so the game has no brake hold: the driver brings the clutch in
  against the sled and the boost builds as it bites. The core still has
  the hold (`session::Controls::hold_brakes`, `tp_pull run --hold-boost`),
  unused by the game. The scripted rivals launch with a slipping clutch
  (eased back while the engine is below 90 % of its rated speed, for up
  to 12 s), so the engine stays up where its big turbo gives full boost
  while the tires spin up: in a tall gear without that the engine stalls.
  Spinning past their bite, they ease the throttle so the tires catch
  again (`spin_ease_slip`).
- **The nose** (videos of good pulls): up once on the launch, touching
  down softly now and then, light through the middle and down at the
  end. With the weights halfway and the hitch in its top hole the
  tractor does that, and pulls furthest; the weights back keep it on the
  wheelie bar all the way, and the hitch in its lowest hole holds the
  front down hard: neither gives the rear tires their most grip, and
  both pull clearly shorter
  (`WheelieRuns.ASupersportTractorLiftsOnceTouchesDownSoftlyAndEndsDown`).

The sled's box is 3,000 kg, on the 12 t sled about 15 t in all. The
rivals keep one build each: at belofte the 7 l engine on the farm tires
and the short drive (`tractors/supersport_3600_farm.toml`; puller tires
pay only with a well built engine behind them), at promo and top on the
puller tires and the middle drive. The class is not
pulled on the loose sandy clay: at realistic grip neither tire gets its
sled going there, so its seasons pull a second clay-sand instead.

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

ETPC 2026, chapter 4 and chapter 12 (limits), and the June 2026 turbo
letter:

- **3500 kg**; hitch at most 500 mm high, at least 450 mm long.
- **No four-wheel drive** (ETPC 2026 ch. 2.S.1, all these classes).
- Engine: at most **8364 cm³ (510 ci)**, down to 2 % smaller; an OEM
  agricultural head of the engine's make, **2 valves a cylinder** (more
  only where the make sold it so), no overhead cams. Steel recast or
  billet heads to OEM dimensions allowed.
- **One stage, one turbo**: compressor wheel at most **129.5 mm (5.1")**,
  turbine wheel at most **133.4 mm (5.25")**; all air through a 132.3 mm
  opening.
- **Diesel only**; alcohol forbidden. Intercooler and water injection
  allowed. One fuel pump of any size, one element a cylinder (common rail
  allowed).
- Only tractors sold as front-wheel-steered farm tractors. The OEM clutch
  housing, gearbox, rear axle and final drives (planetaries stay).
  Component chassis allowed, with conditions.
- **The hood and grille where the maker put them; the stock look stays.**
- Wheelbase at most 2900 mm (or the original, if longer); nothing more
  than 4000 mm ahead of the rear axle.
- A safety frame; the hitch and roll cage part of the frame.
- Tires: rims at most **38"**; at most **650/75R32, 620/75R34 or
  580/70R38**, at most 5334 mm round at 0.7 bar and **635 mm (25") of
  tread**: narrower than Supersport's.
- The NTTO pulls Pro Stocks in two groups (I and II) with promotion and
  relegation (see "Levels").

### 8. Limited Super Stock

ETPC 2026, chapter 8 and chapter 12, and the January 2026 restrictor
letter:

- **3500 kg**; hitch as Pro Stock.
- Engine: **methanol only**, at most **7636 cm³ (466 ci)**.
- **One stage, one turbo** of free size, all air through an **ETPC-made
  100 mm restrictor** at most 100 mm ahead of the compressor wheel (from
  2026; it limits the power).
- Chassis, look, frame and tires as Pro Stock.

### 9. Super Stock

ETPC 2026, chapter 3 and chapter 12:

- **3500 kg**; hitch as Pro Stock.
- Engine: at most **10650 cm³ (650 ci)**, the block an OEM part; **2
  valves a cylinder** unless the model came with 4; a diesel may be
  converted to two stroke.
- **Up to three stages of boost with at most four turbos.**
- Fuel: the class sets no limit of its own, so the general rule holds
  (ETPC 2026 ch. 2.L): **alcohol (methanol), diesel**, petrol or aviation
  and turbine fuel; no oxygen carriers or combustion accelerators. Water
  injection allowed. Turbocharged alcohol tractors need a stainless
  firewall.
- **The hood and grille where the maker put them**, sheet metal of stock
  length matching the engine's make, and the stock look kept (ch. 3.B);
  the grille may be opened for air but not removed (ch. 3.A.13). The
  chassis stays stock from the back of the engine block to the rear.
- Tires: at most **30.5" or 800/65 wide**, a 32" rim over 24.5" wide, at
  most 5400 mm round at 0.7 bar and 788 mm of tread.
- Frame, hitch and roll cage as Pro Stock.

## Levels within a class (rules)

The NTTO splits a class into levels, each its own competition:

- Farmstock classes in 2026: **topniveau**, **promoniveau** and
  **beloftenniveau** (ntto.nl, 2026 standings: Sport 4.5 t and
  Supersport 3.6 t have all three; Sport at 2600 kg, Supersport 3.4 and
  4.5 t top and promo).
- Pro Stocks (NTTO 2018 art. 1.16): **group I and group II**. A new
  machine starts in group II. The **winner of group II moves up** to group
  I and **12th of group I moves down**. Nobody moves up or down by choice,
  and group I is filled up to 12 from group II.

### In the game (game choice)

A class is pulled at up to three levels, **belofte**, **promo** and
**top**, each a season with its own field of rivals, on the class's one
tractor:

- **Twelve in the competition, two guests.** Each level's competition is
  the player and eleven named rivals, the same all season. At every event
  two guests (bijschrijvers) pull too, two of a pool of six drawn afresh
  per event, so each pulls a few events a season. A guest may be anyone
  from the field's weakest to a little above its best (at a class's top
  level up to a fully built tractor), so a guest may win or finish last.
  Guests take their place and its prize money at the event, but no
  points: the points go by place among the competitors, so a guest who
  wins leaves the best competitor the 20 points. A guest who would have
  made the pull-off among the competitors alone (a full pull, or further
  than the fourth competitor) pulls off too, a fifth.
- A new tractor (every part at its first stage) finishes **around last**
  at the class's first level; a **fully upgraded** one **wins at the
  class's last level**
  (tests/data/rival_difficulty_test.cpp).
- **A promotion opens the level above at once.** Finishing a level's
  championship first or second opens the level above (at a class's last
  level the next class's first), in any season, the first too
  (`min_seasons = 1` in the career file, `career::Ladder`; the core can
  still ask more seasons on a rung). The player is asked whether to move up or
  stay. No relegation (for now).
- The money is set so that the rule rarely has to hold anyone back: each
  part's stages dearer as they go, each level paying about 30 % more than
  the one below. Every place pays, so a new tractor at the back still
  climbs: in the standard class, the career's start, the last (14th) earns the
  garage's cheapest upgrade every event, in the Sport classes one every
  two events. Within a class each level pays every place at least about
  10 % more than the level below, so moving up never cuts a back-marker's
  income. The standard class pays the most for its garage, so the first
  upgrades come quickly. A player who buys as they earn needs about three
  seasons for belofte and two for promo and top (an estimate from the
  measured places; playing will tell).
- **The standard class has one level**, the career's warming-up for the
  controls: its few parts (pump, tires, weights, drawbar, governor,
  powershift) are too few for three levels of good play. Its garage
  costs about € 20,000; a new tractor finishes near the back, two thirds
  of the garage fights for the win with the strongest two or three
  rivals, and a player who buys as they earn wins it in
  about two seasons.
- Within a class the tractor stays as it is; a new class is a new tractor
  (see "Every class has its own tractor").
- **The player picks where to pull each season.** When a new season
  starts with more than one class or level open, a career screen lists
  them all (the highest open picked); the player may go back to a lower
  level or class, a Standard 3.5 t season after unlocking Sport 4.5 t
  too. Each class keeps its own tractor, as it was left, and the money is
  one wallet.
- The NTTO's standard class has no levels (nor has the game's) and Pro
  Stock has groups II and I: the game uses the Farmstock levels for the
  other classes alike, except **Sport 2.5 t, only promo and top**: it is the introduction to
  the Farmstock classes and should not hold the player for six seasons
  or more. A new Sport tractor finishes at the back of promo and fights
  for the win with about half its garage; only a fully built one wins at
  top.
- Sport 4.5 t has all three again. Being a wheelspin class near its
  limits at top, about 90 % of its garage reaches the podium there and
  only a fully built tractor is sure to win; its governor comes in six
  small stages, since each 75 rpm is worth a metre or more there.
- Supersport 3.6 t has all three. A new tractor is at the back of
  belofte and about 70 % of its garage wins it; half the garage is mid
  promo and three quarters is in the top five at its damp opener; at top 60 % is at the back and a
  nearly or fully built tractor wins. Its garage costs about € 151,200
  for both engines, both tires, the three final drives and the turbo, so
  its prize money is a fifth (belofte) to two fifths (top) higher than
  it was before the turbo: € 3,110, 3,890 and 5,430 for a win.

## Upgrades (game choice)

**No powershift from Supersport on.** The standard and Sport classes may
shift down under load (the garage's powershift step). The more powerful
classes, from Supersport up, do not: they pull in one gear and rely on
wheelspin, the driver working the throttle to keep the tires spinning at
their best slip. Their garages offer no powershift, and their tractors'
drivelines have none.

The garage offers more stages per part than today's three or four, so
there is something to buy after nearly every event, and more parts in the
classes that allow more: the standard class's stock tractor allows little
(pump, tires, weights, drawbar, governor, powershift), Sport adds the
turbo and its restrictor, the intercooler and the gearing, Supersport the
head and valves, the fuel pump and water injection, and the Grand
National classes the most (the turbo stages in Super Stock). The garage's
limits (`garage::kMaxUpgrades`, `garage::kMaxLevels`) grow to fit.

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
  air restrictors, fuel. These need to become class limits too, so the
  garage can tell whether a part moved from another tractor is legal.
- No class above Standard limits power directly. Each class's engine data
  needs a believable torque curve for its limits (a 7 l Sport engine at
  2700 rpm, a 10.6 l three-stage Super Stock), estimated, not from the
  rules.
- Tire width and rim size are class limits from Sport on, so tires need
  their real size as a part (see `future_improvements.md`).
- The faster classes need sled drag that grows with speed.
