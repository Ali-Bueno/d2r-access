# 01 — Coordinate System & Math Primitives

This is the foundation. Every other document assumes these conventions.

---

## 1. Vectors: `V2` / `V3`

`Core/Ian/V2.cs`, `V3.cs` — standard OpenTK-style structs (`public float X, Y[, Z]`). Nothing exotic;
the only members navigation uses heavily:

`Core/Ian/V2.cs` provides Euclidean length, squared length, a fast length estimate, copy/in-place normalization, perpendiculars, dot product and the signed 2D cross product. `V3.cs` adds the third coordinate. The right perpendicular swaps X/Y and negates the original X; the left perpendicular negates the original Y. These are the members used by navigation; see the later basis examples before interpreting the names in screen coordinates.

Equality is **exact float equality** (`==` calls `Equals`, compares X and Y with `==`). The radar and
movement code therefore use a separate epsilon comparison (below) for "almost equal".

`CoreExtensionMethods.AlmostEquals` uses an absolute scalar tolerance of `1e-5`. The vector overloads in `SoundExtensionMethods.cs` require that comparison independently for each component. `CoreExtensionMethods.AsV3` preserves X and Y and supplies zero Z.

> **Porting note:** in Unity/most engines you'll use `UnityEngine.Vector2`/`Vector3`. The only thing
> you must preserve is the **Y-down, compass-clockwise** convention used by the angle math below, or
> consistently flip it everywhere. Mixing conventions is the #1 source of "left/right is mirrored" bugs.

---

## 2. The compass-angle system

The world uses a **compass heading**: degrees clockwise, `0° = North`, and the world is **Y-down**
(`+Y` = South). Three conversion functions matter.

### Heading (degrees) → direction unit vector — the one you'll use most

`MyMath.GetUnitVectorFromCompassDegrees` converts degrees to radians, takes sine for X and negative cosine for Y, then normalizes. North, East, South and West therefore point along negative Y, positive X, positive Y and negative X respectively; diagonal headings produce equal-magnitude components.

`VectorHelpers.cs` has the same math in three flavours (pick by your engine's Y direction):

`VectorHelpers.cs` offers three distinct helpers: `FromCompassDegrees` uses Y-up compass coordinates; `FromFlippedYCompassDegrees` uses the game's Y-down compass frame; `FromMathDegrees` uses the usual mathematical counterclockwise frame starting at East. Select by coordinate convention, not helper-name similarity.

### Direction unit vector → heading (degrees) — `GetCompassAngle` is NOT the inverse you want

`VectorExtensions.GetCompassAngle` discards Z, normalizes the remaining direction, derives an angle from the Y component with inverse cosine and uses the X sign to choose the hemisphere. It assumes positive Y is North, unlike the Y-down heading conversion above.

> **Correction:** this function treats `+Y = North` — i.e. it's **Y-up**, the odd one out. It is the
> inverse of `VectorHelpers.FromCompassDegrees` (the Y-up flavour, `y = +cos`), **not** of the Y-down
> `GetUnitVectorFromCompassDegrees`/`FromFlippedYCompassDegrees` the rest of the game (and this doc)
> uses. Feed it a Y-down direction vector and you get a **mirrored** angle. Evidence:
> `VectorExtensions.cs:7-14` vs `MyMath.cs:35-43`. In practice this is harmless: `GetCompassAngle`'s
> **sole caller** is `VectorHelpers.MyTransform` (`VectorHelpers.cs:38`), which is itself used **only in
> sound-relative transforms** (`FPLogic.cs:234` `MakeRelativeToPlayerFacingAndPosition`;
> `MapSoundHandler.cs:216`) — **never** on the movement/heading path. The actual bearing readout (below)
> computes `atan2` directly instead of calling this function.
> If you need a real "vector → Y-down compass heading" inverse when porting, derive it yourself from
> `atan2`, don't reuse `GetCompassAngle`.

There is also `GetMathAngleInDegrees` (angle from +X, CCW) if you ever need standard math angles.

### The beacon bearing (used to say "45, NorthEast")

`FPExploring` computes the bearing from a delta vector with `atan2` directly:

`FPExploring` starts with the target-to-player delta, derives a compass angle using X and negated Y as the inverse-tangent inputs, converts to degrees and adds a half turn. `DirectionHelper.GetRoughDirectionEnum` then selects the eight-point direction; see its bin boundaries in study 07.

> **Why `atan2(X, -Y)`?** Standard `atan2(y, x)` gives the math angle (0=East, CCW). Passing
> `(X, -Y)` swaps/negates so that 0 means North and the angle grows clockwise — i.e. a compass
> bearing — for a Y-down world. Memorize this; you will reuse it for any "which way is target" readout.

---

## 3. Degrees / radians helpers

`MyMath.DegreesToRadians` scales by pi per 180 degrees; `RadiansToDegrees` applies the inverse scale. See `MyMath.cs` for the float conversion behavior.

---

## 4. The eight direction vectors (player-relative)

`Sound/Ian/MapAndPlayer.cs` — the per-frame bundle handed to the radar. `Front` is precomputed from
the heading; the rest are derived. **This is the exact basis the radar and scans use.**

`Sound/Ian/MapAndPlayer.cs` stores Front from the compass conversion. Left uses Front's right-perpendicular member and Right its left-perpendicular member in this Y-down convention; Back reverses Front. Each diagonal normalizes the sum of its adjacent cardinal basis vectors. The worked examples below establish the intended handedness.

Worked example, facing **East** (`Facing = 90°`, `Front = (1,0)`):
- Left is North, vector (0,-1), from the right perpendicular of East → North. ✓ (East-facing player's left hand points North)
- Right is South, vector (0,1), from the left perpendicular of East → South. ✓
- FrontLeft is the normalized Northeast diagonal, approximately (0.707,-0.707) → NorthEast. ✓

> **Two derivations of the same basis (don't let it confuse you).** The literal `Perpendicular*`
> properties above are what the **sound module** uses (`Sound/Ian/MapAndPlayer.cs:19,23` —
> Left uses Front.PerpendicularRight, Right uses Front.PerpendicularLeft). The **navigation /
> first-person side** computes the identical basis via **cross products** instead:
> Left uses the XY projection of Front crossed with Up, Right uses the XY projection of Up crossed with Front
> (`FirstPerson/Ian/FPClientState.cs:39,43`, with `Up = (0,0,1)`). With that up vector,
> `Cross(Front, Up).Xy == (Front.Y, -Front.X) == PerpendicularRight` and
> `Cross(Up, Front).Xy == (-Front.Y, Front.X) == PerpendicularLeft` — algebraically the same result.
> They are two spellings of one convention, not two conventions; port either.

> If your engine is Y-up, either negate the Y of every direction vector once at the adapter boundary,
> or swap `Left`/`Right` definitions. Verify with the worked example above in your own frame before
> trusting any radar output.

---

## 5. World grid: tile size, storage & padding (missing from earlier pass — added)

- **Tile size = 1.0 world unit.** Tile `(i,j)` occupies `[i,i+1) × [j,j+1)`; `Center=(i+0.5, j+0.5)`,
  Bounds set to the result of RectangleF with arguments X, Y, 1, 1 (`FPTile.cs:14-21`). the player tile obtained by flooring X and Y independently
  (`FPMapLogic.GetTile`, `FPMapLogic.cs:31`).
- **Tile storage is column-major**, not row-major: `TileCollection<T>`'s indexer is
  column-major lookup in m_tiles with Height stride and Y as the inner index (`GameEngine/Ian/TileCollection.cs:21,28`). Only matters if you
  ever touch the backing array directly instead of `Get(x,y)`/`Set(x,y,..)`.
- **The map is padded, and every coordinate in the game lives in the padded frame.** `PaddedWidth =
  Width+2`, `PaddedHeight = Height+2` (`FPMapType.cs:357-359`). At load, a 1-tile **border ring** is
  force-set to a terrain looked up **by name** `"map boundary"` from the game's terrain data
  (`FPMapType.SetupTiles`, `FPMapType.cs:130-132`; padding at `:357-359`). **Its blocking behaviour is
  data-defined, not a hardcoded flag:** whether that ring is `Wall`/`Impassable` comes from the
  `"map boundary"` terrain's own data (see the Doc 03 §latent-bug note — a ray only terminates cleanly at
  the ring because that terrain happens to carry the `Wall` flag).
  The logical/authored map is tiles `[1..Width]×[1..Height]`; the map-review grid iterates `1..Width`
  and calls that "the edge of the map" (`FPMapGridBase.cs:60-108`). **All raycasters and bounds checks
  (Doc 03) test against the Padded dimensions**, so a ray or the player naturally stops at that border
  tile rather than at `Width`/`Height`. When porting, either mirror the padding (simplest — a ray always
  terminates cleanly) or make sure your own bounds checks account for the +1 offset this implies for any
  coordinate recovered from the source.

---

## 6. `Line` — analytic line / segment geometry

`Sound/Ian/Line.cs`. The raycaster and the radar's "open vs closed" test use this. A line is stored
in implicit form `A·x + B·y = C` plus its two defining points and a normalized `Slope` (a direction).

`Sound/Ian/Line.cs` stores an implicit line equation, two defining points and their normalized direction. `FromPoints` derives the coefficients from coordinate differences; `SolveForX` and `SolveForY` solve one coordinate at a time. `IsPointLeftOfLine` tests the signed 2D cross product. `Equivalent` accepts approximately equal or opposite slopes, so it checks parallel orientation without proving equal offset. Horizontal and vertical flags use exact zero slope components.

Two members are the workhorses elsewhere:
- **`SolveForX` / `SolveForY`** drive the grid raycaster (Doc 03): step across each integer gridline and
  solve for the crossing on the other axis.
- **`IsPointLeftOfLine`** drives the radar's open/closed decision (Doc 02): compare which side of the
  previous wall-edge line the player vs the new collision point fall on.

`DistanceToPoint`, `GetIntersection`, and segment-range clamping also exist but navigation doesn't use
them on the hot path.

---

## 7. Quick reference card

Coordinate reference: `V2.cs`, `MyMath.cs`, `MapAndPlayer.cs` and `Line.cs`. Tiles use half-open unit squares with half-unit centers; player tiles use floor coordinates. Storage is column-major with height stride. The padded frame adds one data-defined boundary tile around each map edge. Positive X is East and positive Y South; compass headings increase clockwise from North. Use the flipped-Y compass conversion for travel and the target-to-player delta plus half-turn convention for beacon bearings. The vector comparison tolerance is `1e-5`; `GetCompassAngle` is the Y-up exception, not the inverse of the travel conversion.
