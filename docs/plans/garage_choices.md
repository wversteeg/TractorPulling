# Plan: garage choices, and Sport 4.5 t as the setup class

Status: approved; steps 1 to 5 built. Supersport 3.6 t ("Later") is
still to come. Remove this file once it is built (or move what is left
to `docs/future_improvements.md`).

What came out differently from the plan:

- The small turbo is the heavy-clay turbo: it wins by metres on the
  covered heavy clay, the 68 mm one by a metre or three on every firmer
  track (`TurboChoice.NeitherTurboWinsOnEveryTrack`). Tuned so a
  half-built tractor on it is no stronger in the field than before
  (`RivalDifficulty.*`).
- For the scripted pullers the hitch's top hole always pulls furthest;
  the cost of a high hitch (the front up, the steering gone) is the
  player's only (`docs/future_improvements.md`).
- "Keeping the turbo lit" is the engine's existing turbo model
  (`na_torque_fraction`, the lag); it was not tuned further.

## Why

The career should get a little harder at every class:

| Class | What it teaches |
|---|---|
| Standard 3.5 t | The controls. |
| Sport 2.5 t | A careful clutch, weight distribution, wheelies. |
| **Sport 4.5 t** | **Setup and trade-offs**: the first parts that are not simply better, and a tractor set up for each track. |
| Supersport 3.6 t | The big swaps: engine (7.0 l at 2700 rpm or 6.06 l at 3200 rpm), and tires plus gearbox (traction or wheelspin). Roll cage and helmet. |
| Supersport 4.5 t | As 3.6 t, plus engine tuning. |

Today Sport 4.5 t is Sport 2.5 t made bigger: the same parts, more steps,
more power. Every garage level is better than the one below, so there is
nothing to choose. Supersport needs parts the player owns side by side and
switches between, both ways. Sport 4.5 t is where that garage arrives, with
one small choice (the turbo) before Supersport's big ones.

Supersport 3.4 t is left out (game choice): its rules allow three engines
of four cylinders, and 3.6 t tells the two-engine story more plainly, the
7.0 l being the Sport 4.5 t engine's size. It can be added later.

## What the player sees

### The garage

Two kinds of rows:

- **Upgrades**, as now: levels bought one after the other, each better
  (pump, injectors, head, weights...).
- **Choices** (new): a slot with two to four alternatives, for example
  *Turbo: fast turbo | 68 mm turbo*. The first alternative is the one the
  tractor comes with. The player buys another one; it is then **owned**
  and can be **fitted** or taken off again for free, as often as they
  like, between events (not during one, as buying).

An upgrade may belong to one alternative: Supersport's "6.0 l build"
levels count only while that engine is fitted, and stay bought while it
is off. Sport 4.5 t's engine build counts on either turbo.

### Sport 4.5 t

1. **Turbo** (a choice):
   - *Fast turbo* (the one it comes with): boost from low revs, little
     lag; less top power. For grippy tracks and heavy starts.
   - *68 mm turbo* (bought): little boost low down and more lag, but the
     full 520 hp. Fast, but it falls off its boost if the revs sag.
2. **Pulling hitch set per pull**: the setup screen gets a third slider,
   the hitch's height, beside the weights and the tire pressure. The
   garage's drawbar upgrade sets how high it can go (up to the class's
   0.50 m); the player picks the hole each pull. Higher puts more weight
   on the rear tires but lifts the nose sooner.
3. **Keeping the turbo lit**: tuned so that letting the revs fall below
   the boost range (or shifting down late) loses the pull, and a timely
   shift down saves it. The physics is there (`na_torque_fraction`,
   the turbo lag); this is data and tuning, no new controls.

## How it is built

Each step is one PR with its tests, diagrams (`garage_classes.puml` and
the setup flow) and requirements.

### 1. Core: choices in the garage (`tp::garage`)

- `Choice`: what it swaps (`Slot::kEngine`, `kRearTire`, `kDriveline`),
  up to `kMaxAlternatives` (4) alternatives, each the whole component's
  params (`engine::Params`, `tire::Params`, `driveline::Params`) and its
  cost (the first free). Plain data in the `Catalogue`, beside the
  upgrades (`kMaxChoices`, 4).
- `Upgrade` gains an optional owner: (choice, alternative). It applies
  only while that alternative is fitted.
- `Career` gains, per choice, the alternatives owned (a bit mask) and
  the one fitted.
- `equipped()`: start from the tractor, put in each fitted alternative,
  then apply the upgrades in order (so the torque scale and the governor
  work on whichever engine is fitted).
- `buy_alternative()`, `fit()`: buying needs the money; fitting needs it
  owned. Neither during an event (as `buy`, enforced by the caller as
  today).
- `validate()`: each class limit still reads one part alone, so every
  combination of fitted alternatives (at most 4^4) is checked with each
  upgrade level and the others at their top. Tests: owned and fitted
  stay apart; taking a part off keeps its levels; an owned upgrade does
  nothing while its alternative is off; validation catches an
  alternative outside the class.

### 2. Data: the garage file and the save (`tp::data`)

- Garage TOML gains `[[garage.choices]]`: `slot`, `name`, and
  `alternatives = [{ file = "../engines/sport_4500_fast_turbo.toml",
  cost_eur = 0, label = "Fast turbo" }, ...]`; an upgrade may say
  `for = "68 mm turbo"`. Each file loads with its own loader and
  validation, errors naming the garage file and field.
- Save version 5: `[[career.choices]] name, fitted, owned = [...]`, by
  name as the upgrades are, so a save outlives a reordered file; a
  version 4 save reads as every choice at its first alternative.
- Moving up a class carries choices by name, as upgrades.

### 3. The game: the garage screen (extension and GDScript)

- `get_garage()` lists choices: name, alternatives with label, cost,
  owned, fitted. `buy_alternative(choice, alt)` and
  `fit_alternative(choice, alt)` return "" or why not.
- The garage screen shows a choice as a row of buttons: an owned one
  fits it, one not owned shows its price. Upgrades belonging to an
  alternative that is off show greyed, "for the 68 mm turbo".
- Flow test: buy, fit, take off, save and load.

### 4. The hitch set per pull (`vehicle::Tuning`)

- `Tuning::hitch_height_m`; the tractor file gains the lowest hole,
  `hitch_min_height_m`; the garage's drawbar sets the highest. `tuned()`
  holds it to that range. As built is the top hole, so today's runs and
  hashes stay the same.
- Setup screen: the third slider, shown where the tractor has a range
  (the Sport classes up).
- Save version 5 (the same version as step 2) keeps it; `tp_pull`
  gains `--hitch M`.

### 5. Sport 4.5 t's data and tuning

- `engines/sport_4500_fast_turbo.toml` and the present engine as the
  68 mm turbo: boost onset and full boost lower, a shorter time
  constant, a curve with more torque low and less on top (about 450 hp).
- `garages/sport_4500.toml`: the turbo choice, the drawbar as the
  hitch's range.
- Tuning by `tp_pull compare-tracks` and the scripted pullers: on a
  grippy track the fast turbo pulls further, on a loose one the 68 mm
  turbo; neither wins everywhere. Tests assert that (as the existing
  "how the Sport classes pull" tests do), plus: falling below boost
  loses the pull, a timely shift down saves it.
- `RivalDifficulty` stays met (K-3): fully built still wins at Top.
- `docs/career.md` updated, the scripted runs' hashes refreshed where
  the data changes them.

### Later: Supersport 3.6 t

Its own plan, on this garage: the class, tractor, roll cage, the engine
choice (7.0 l at 2700 rpm with its build, 6.06 l at 3200 rpm with
another), and the tires plus gearbox choice (traction or wheelspin),
then Supersport 4.5 t with engine tuning.

## Requirements to add

- **K-6** A garage choice can be bought, fitted and taken off again
  between events; what is bought stays bought.
- **K-7** No choice is best everywhere: Sport 4.5 t's turbos each win on
  some of its tracks.
- **R-x** The hitch's height is set before each pull within the
  tractor's range (the class's limit at most).

## Open questions

1. Switching a fitted part: free (proposed), or a small fitting fee?
2. Should the rivals use the choices too (a rival fitting the turbo that
   suits the track), or keep one fixed build each? Proposed: fixed builds
   for now, with some rivals on each turbo.
