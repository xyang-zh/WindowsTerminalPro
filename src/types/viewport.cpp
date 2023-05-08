// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Viewport.hpp"

using namespace Microsoft::Console::Types;

Viewport::Viewport(const til::inclusive_rect& sr) noexcept :
    _sr(sr)
{
}

Viewport Viewport::Empty() noexcept
{
    return Viewport();
}

Viewport Viewport::FromInclusive(const til::inclusive_rect& sr) noexcept
{
    return Viewport(sr);
}

Viewport Viewport::FromExclusive(const til::rect& sr) noexcept
{
    return FromInclusive(sr.to_inclusive_rect());
}

// Function Description:
// - Creates a new Viewport at the given origin, with the given dimensions.
// Arguments:
// - origin: The origin of the new Viewport. Becomes the Viewport's Left, Top
// - width: The width of the new viewport
// - height: The height of the new viewport
// Return Value:
// - a new Viewport at the given origin, with the given dimensions.
Viewport Viewport::FromDimensions(const til::point origin,
                                  const til::CoordType width,
                                  const til::CoordType height) noexcept
{
    return Viewport(til::inclusive_rect{
        origin.x,
        origin.y,
        origin.x + width - 1,
        origin.y + height - 1,
    });
}

// Function Description:
// - Creates a new Viewport at the given origin, with the given dimensions.
// Arguments:
// - origin: The origin of the new Viewport. Becomes the Viewport's Left, Top
// - dimensions: A coordinate containing the width and height of the new viewport
//      in the x and y coordinates respectively.
// Return Value:
// - a new Viewport at the given origin, with the given dimensions.
Viewport Viewport::FromDimensions(const til::point origin,
                                  const til::size dimensions) noexcept
{
    return Viewport(til::inclusive_rect{
        origin.x,
        origin.y,
        origin.x + dimensions.width - 1,
        origin.y + dimensions.height - 1,
    });
}

// Function Description:
// - Creates a new Viewport at the origin, with the given dimensions.
// Arguments:
// - dimensions: A coordinate containing the width and height of the new viewport
//      in the x and y coordinates respectively.
// Return Value:
// - a new Viewport at the origin, with the given dimensions.
Viewport Viewport::FromDimensions(const til::size dimensions) noexcept
{
    return FromDimensions({}, dimensions);
}

// Method Description:
// - Creates a Viewport equivalent to a 1x1 rectangle at the given coordinate.
// Arguments:
// - origin: origin of the rectangle to create.
// Return Value:
// - a 1x1 Viewport at the given coordinate
Viewport Viewport::FromCoord(const til::point origin) noexcept
{
    return FromInclusive(til::inclusive_rect{ origin.x, origin.y, origin.x, origin.y });
}

til::CoordType Viewport::Left() const noexcept
{
    return _sr.left;
}

til::CoordType Viewport::RightInclusive() const noexcept
{
    return _sr.right;
}

til::CoordType Viewport::RightExclusive() const noexcept
{
    return _sr.right + 1;
}

til::CoordType Viewport::Top() const noexcept
{
    return _sr.top;
}

til::CoordType Viewport::BottomInclusive() const noexcept
{
    return _sr.bottom;
}

til::CoordType Viewport::BottomExclusive() const noexcept
{
    return _sr.bottom + 1;
}

til::CoordType Viewport::Height() const noexcept
{
    return BottomExclusive() - Top();
}

til::CoordType Viewport::Width() const noexcept
{
    return RightExclusive() - Left();
}

// Method Description:
// - Get a coord representing the origin of this viewport.
// Arguments:
// - <none>
// Return Value:
// - the coordinates of this viewport's origin.
til::point Viewport::Origin() const noexcept
{
    return { Left(), Top() };
}

// Method Description:
// - Get a coord representing the bottom right of the viewport in inclusive terms.
// Arguments:
// - <none>
// Return Value:
// - the inclusive bottom right coordinates of this viewport.
til::point Viewport::BottomRightInclusive() const noexcept
{
    return { RightInclusive(), BottomInclusive() };
}

// Method Description:
// - Get a coord representing the bottom right of the viewport in exclusive terms.
// Arguments:
// - <none>
// Return Value:
// - the exclusive bottom right coordinates of this viewport.
til::point Viewport::BottomRightExclusive() const noexcept
{
    return { RightExclusive(), BottomExclusive() };
}

// Method Description:
// - For Accessibility, get a til::point representing the end of this viewport in exclusive terms.
// - This is needed to represent an exclusive endpoint in UiaTextRange that includes the last
//    til::point's text in the buffer at (RightInclusive(), BottomInclusive())
// Arguments:
// - <none>
// Return Value:
// - the coordinates of this viewport's end.
til::point Viewport::EndExclusive() const noexcept
{
    return { Left(), BottomExclusive() };
}

// Method Description:
// - Get a coord representing the dimensions of this viewport.
// Arguments:
// - <none>
// Return Value:
// - the dimensions of this viewport.
til::size Viewport::Dimensions() const noexcept
{
    return { Width(), Height() };
}

// Method Description:
// - Determines if the given viewport fits within this viewport.
// Arguments:
// - other - The viewport to fit inside this one
// Return Value:
// - True if it fits. False otherwise.
bool Viewport::IsInBounds(const Viewport& other) const noexcept
{
    return other.Left() >= Left() && other.Left() <= RightInclusive() &&
           other.RightInclusive() >= Left() && other.RightInclusive() <= RightInclusive() &&
           other.Top() >= Top() && other.Top() <= other.BottomInclusive() &&
           other.BottomInclusive() >= Top() && other.BottomInclusive() <= BottomInclusive();
}

// Method Description:
// - Determines if the given coordinate position lies within this viewport.
// Arguments:
// - pos - Coordinate position
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it lies inside the viewport. False otherwise.
bool Viewport::IsInBounds(const til::point pos, bool allowEndExclusive) const noexcept
{
    if (allowEndExclusive && pos == EndExclusive())
    {
        return true;
    }

    return pos.x >= Left() && pos.x < RightExclusive() &&
           pos.y >= Top() && pos.y < BottomExclusive();
}

// Method Description:
// - Clamps a coordinate position into the inside of this viewport.
// Arguments:
// - pos - coordinate to update/clamp
// Return Value:
// - <none>
void Viewport::Clamp(til::point& pos) const
{
    THROW_HR_IF(E_NOT_VALID_STATE, !IsValid()); // we can't clamp to an invalid viewport.

    pos.x = std::clamp(pos.x, Left(), RightInclusive());
    pos.y = std::clamp(pos.y, Top(), BottomInclusive());
}

// Method Description:
// - Clamps a viewport into the inside of this viewport.
// Arguments:
// - other - Viewport to clamp to the inside of this viewport
// Return Value:
// - Clamped viewport
Viewport Viewport::Clamp(const Viewport& other) const noexcept
{
    auto clampMe = other.ToInclusive();

    clampMe.left = std::clamp(clampMe.left, Left(), RightInclusive());
    clampMe.right = std::clamp(clampMe.right, Left(), RightInclusive());
    clampMe.top = std::clamp(clampMe.top, Top(), BottomInclusive());
    clampMe.bottom = std::clamp(clampMe.bottom, Top(), BottomInclusive());

    return Viewport::FromInclusive(clampMe);
}

// Method Description:
// - Moves the coordinate given by the number of positions and
//   in the direction given (repeated increment or decrement)
// Arguments:
// - move - Magnitude and direction of the move
// - pos - The coordinate position to adjust
// Return Value:
// - True if we successfully moved the requested distance. False if we had to stop early.
// - If False, we will restore the original position to the given coordinate.
bool Viewport::MoveInBounds(const til::CoordType move, til::point& pos) const noexcept
{
    const auto backup = pos;
    auto success = true; // If nothing happens, we're still successful (e.g. add = 0)

    for (til::CoordType i = 0; i < move; i++)
    {
        success = IncrementInBounds(pos);

        // If an operation fails, break.
        if (!success)
        {
            break;
        }
    }

    for (til::CoordType i = 0; i > move; i--)
    {
        success = DecrementInBounds(pos);

        // If an operation fails, break.
        if (!success)
        {
            break;
        }
    }

    // If any operation failed, revert to backed up state.
    if (!success)
    {
        pos = backup;
    }

    return success;
}

// Method Description:
// - Increments the given coordinate within the bounds of this viewport.
// Arguments:
// - pos - Coordinate position that will be incremented, if it can be.
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be incremented. False if it would move outside.
bool Viewport::IncrementInBounds(til::point& pos, bool allowEndExclusive) const noexcept
{
    return WalkInBounds(pos, { XWalk::LeftToRight, YWalk::TopToBottom }, allowEndExclusive);
}

// Method Description:
// - Increments the given coordinate within the bounds of this viewport
//   rotating around to the top when reaching the bottom right corner.
// Arguments:
// - pos - Coordinate position that will be incremented.
// Return Value:
// - True if it could be incremented inside the viewport.
// - False if it rolled over from the bottom right corner back to the top.
bool Viewport::IncrementInBoundsCircular(til::point& pos) const noexcept
{
    return WalkInBoundsCircular(pos, { XWalk::LeftToRight, YWalk::TopToBottom });
}

// Method Description:
// - Decrements the given coordinate within the bounds of this viewport.
// Arguments:
// - pos - Coordinate position that will be incremented, if it can be.
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be incremented. False if it would move outside.
bool Viewport::DecrementInBounds(til::point& pos, bool allowEndExclusive) const noexcept
{
    return WalkInBounds(pos, { XWalk::RightToLeft, YWalk::BottomToTop }, allowEndExclusive);
}

// Method Description:
// - Decrements the given coordinate within the bounds of this viewport
//   rotating around to the bottom right when reaching the top left corner.
// Arguments:
// - pos - Coordinate position that will be decremented.
// Return Value:
// - True if it could be decremented inside the viewport.
// - False if it rolled over from the top left corner back to the bottom right.
bool Viewport::DecrementInBoundsCircular(til::point& pos) const noexcept
{
    return WalkInBoundsCircular(pos, { XWalk::RightToLeft, YWalk::BottomToTop });
}

// Routine Description:
// - Compares two coordinate positions to determine whether they're the same, left, or right within the given buffer size
// Arguments:
// - first- The first coordinate position
// - second - The second coordinate position
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// -  Negative if First is to the left of the Second.
// -  0 if First and Second are the same coordinate.
// -  Positive if First is to the right of the Second.
// -  This is so you can do s_CompareCoords(first, second) <= 0 for "first is left or the same as second".
//    (the < looks like a left arrow :D)
// -  The magnitude of the result is the distance between the two coordinates when typing characters into the buffer (left to right, top to bottom)
#pragma warning(suppress : 4100)
int Viewport::CompareInBounds(const til::point first, const til::point second, bool allowEndExclusive) const noexcept
{
    // Assert that our coordinates are within the expected boundaries
    assert(IsInBounds(first, allowEndExclusive));
    assert(IsInBounds(second, allowEndExclusive));

    // First set the distance vertically
    //   If first is on row 4 and second is on row 6, first will be -2 rows behind second * an 80 character row would be -160.
    //   For the same row, it'll be 0 rows * 80 character width = 0 difference.
    auto retVal = (first.y - second.y) * Width();

    // Now adjust for horizontal differences
    //   If first is in position 15 and second is in position 30, first is -15 left in relation to 30.
    retVal += (first.x - second.x);

    // Further notes:
    //   If we already moved behind one row, this will help correct for when first is right of second.
    //     For example, with row 4, col 79 and row 5, col 0 as first and second respectively, the distance is -1.
    //     Assume the row width is 80.
    //     Step one will set the retVal as -80 as first is one row behind the second.
    //     Step two will then see that first is 79 - 0 = +79 right of second and add 79
    //     The total is -80 + 79 = -1.
    return retVal;
}

// Method Description:
// - Walks the given coordinate within the bounds of this viewport in the specified
//   X and Y directions.
// Arguments:
// - pos - Coordinate position that will be adjusted, if it can be.
// - dir - Walking direction specifying which direction to go when reaching the end of a row/column
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be adjusted as specified and remain in bounds. False if it would move outside.
bool Viewport::WalkInBounds(til::point& pos, const WalkDir dir, bool allowEndExclusive) const noexcept
{
    auto copy = pos;
    if (WalkInBoundsCircular(copy, dir, allowEndExclusive))
    {
        pos = copy;
        return true;
    }
    else
    {
        return false;
    }
}

// Method Description:
// - Walks the given coordinate within the bounds of this viewport
//   rotating around to the opposite corner when reaching the final corner
//   in the specified direction.
// Arguments:
// - pos - Coordinate position that will be adjusted.
// - dir - Walking direction specifying which direction to go when reaching the end of a row/column
// - allowEndExclusive - if true, allow the EndExclusive til::point as a valid position.
//                        Used in accessibility to signify that the exclusive end
//                        includes the last til::point in a given viewport.
// Return Value:
// - True if it could be adjusted inside the viewport.
// - False if it rolled over from the final corner back to the initial corner
//   for the specified walk direction.
bool Viewport::WalkInBoundsCircular(til::point& pos, const WalkDir dir, bool allowEndExclusive) const noexcept
{
    // Assert that the position given fits inside this viewport.
    assert(IsInBounds(pos, allowEndExclusive));

    if (dir.x == XWalk::LeftToRight)
    {
        if (allowEndExclusive && pos.x == Left() && pos.y == BottomExclusive())
        {
            pos.y = Top();
            return false;
        }
        else if (pos.x == RightInclusive())
        {
            pos.x = Left();

            if (dir.y == YWalk::TopToBottom)
            {
                pos.y++;
                if (allowEndExclusive && pos.y == BottomExclusive())
                {
                    return true;
                }
                else if (pos.y > BottomInclusive())
                {
                    pos.y = Top();
                    return false;
                }
            }
            else
            {
                pos.y--;
                if (pos.y < Top())
                {
                    pos.y = BottomInclusive();
                    return false;
                }
            }
        }
        else
        {
            pos.x++;
        }
    }
    else
    {
        if (pos.x == Left())
        {
            pos.x = RightInclusive();

            if (dir.y == YWalk::TopToBottom)
            {
                pos.y++;
                if (pos.y > BottomInclusive())
                {
                    pos.y = Top();
                    return false;
                }
            }
            else
            {
                pos.y--;
                if (pos.y < Top())
                {
                    pos.y = BottomInclusive();
                    return false;
                }
            }
        }
        else
        {
            pos.x--;
        }
    }

    return true;
}

// Routine Description:
// - If walking through a viewport, one might want to know the origin
//   for the direction walking.
// - For example, for walking up and to the left (bottom right corner
//   to top left corner), the origin would start at the bottom right.
// Arguments:
// - dir - The direction one intends to walk through the viewport
// Return Value:
// - The origin for the walk to reach every position without circling
//   if using this same viewport with the `WalkInBounds` methods.
til::point Viewport::GetWalkOrigin(const WalkDir dir) const noexcept
{
    til::point origin;
    origin.x = dir.x == XWalk::LeftToRight ? Left() : RightInclusive();
    origin.y = dir.y == YWalk::TopToBottom ? Top() : BottomInclusive();
    return origin;
}

// Routine Description:
// - Given two viewports that will be used for copying data from one to the other (source, target),
//   determine which direction you will have to walk through them to ensure that an overlapped copy
//   won't erase data in the source that hasn't yet been read and copied into the target at the same
//   coordinate offset position from their respective origins.
// - Note: See elaborate ASCII-art comment inside the body of this function for more details on how/why this works.
// Arguments:
// - source - The viewport representing the region that will be copied from
// - target - The viewport representing the region that will be copied to
// Return Value:
// - The direction to walk through both viewports from the walk origins to touch every cell and not
//   accidentally overwrite something that hasn't been read yet. (use with GetWalkOrigin and WalkInBounds)
Viewport::WalkDir Viewport::DetermineWalkDirection(const Viewport& source, const Viewport& target) noexcept
{
    // We can determine which direction we need to walk based on solely the origins of the two rectangles.
    // I'll use a few examples to prove the situation.
    //
    // For the cardinal directions, let's start with this sample:
    //
    // source        target
    // origin 0,0    origin 4,0
    // |             |
    // v             V
    // +--source-----+--target---------                  +--source-----+--target---------
    // |  A  B  C  D | E | 1  2  3  4 |     becomes      |  A  B  C  D | A | B  C  D  E |
    // |  F  G  H  I | J | 5  6  7  8 |    =========>    |  F  G  H  I | F | G  H  I  J |
    // |  K  L  M  N | O | 9  $  %  @ |                  |  K  L  M  N | K | L  M  N  O |
    // --------------------------------                  --------------------------------
    //
    // The source and target overlap in the 5th column (X=4).
    // To ensure that we don't accidentally write over the source
    // data before we copy it into the target, we want to start by
    // reading that column (a.k.a. writing to the farthest away column
    // of the target).
    //
    // This means we want to copy from right to left.
    // Top to bottom and bottom to top don't really matter for this since it's
    // a cardinal direction shift.
    //
    // If we do the right most column first as so...
    //
    // +--source-----+--target---------                  +--source-----+--target---------
    // |  A  B  C  D | E | 1  2  3  4 |     step 1       |  A  B  C  D | E | 1  2  3  E |
    // |  F  G  H  I | J | 5  6  7  8 |    =========>    |  F  G  H  I | J | 5  6  7  J |
    // |  K  L  M  N | O | 9  $  %  @ |                  |  K  L  M  N | O | 9  $  %  O |
    // --------------------------------                  --------------------------------
    //
    // ... then we can see that the EJO column is safely copied first out of the way and
    // can be overwritten on subsequent steps without losing anything.
    // The rest of the columns aren't overlapping, so they'll be fine.
    //
    // But we extrapolate this logic to follow for rectangles that overlap more columns, up
    // to and including only leaving one column not overlapped...
    //
    // source   target
    // origin   origin
    // 0,0    / 1,0
    // |     /
    // v    v
    // +----+------target-                  +----+------target-
    // | A | B  C  D | E |     becomes      | A | A  B  C | D |
    // | F | G  H  I | J |    =========>    | F | F  G  H | I |
    // | K | L  M  N | O |                  | K | K  L  M | N |
    // ---source----------                  ---source----------
    //
    // ... will still be OK following the same Right-To-Left rule as the first move.
    //
    // +----+------target-                  +----+------target-
    // | A | B  C  D | E |     step 1       | A | B  C  D | D |
    // | F | G  H  I | J |    =========>    | F | G  H  I | I |
    // | K | L  M  N | O |                  | K | L  M  N | N |
    // ---source----------                  ---source----------
    //
    // The DIN column from the source was moved to the target as the right most column
    // of both rectangles. Now it is safe to iterate to the second column from the right
    // and proceed with moving CHM on top of the source DIN as it was already moved.
    //
    // +----+------target-                  +----+------target-
    // | A | B  C  D | E |     step 2       | A | B  C  C | D |
    // | F | G  H  I | J |    =========>    | F | G  H  H | I |
    // | K | L  M  N | O |                  | K | L  M  M | N |
    // ---source----------                  ---source----------
    //
    // Continue walking right to left (an exercise left to the reader,) and we never lose
    // any source data before it reaches the target with the Right To Left pattern.
    //
    // We notice that the target origin was Right of the source origin in this circumstance,
    // (target origin X is > source origin X)
    // so it is asserted that targets right of sources means that we should "walk" right to left.
    //
    // Reviewing the above, it doesn't appear to matter if we go Top to Bottom or Bottom to Top,
    // so the conclusion is drawn that it doesn't matter as long as the source and target origin
    // Y values are the same.
    //
    // Also, extrapolating this cardinal direction move to the other 3 cardinal directions,
    // it should follow that they would follow the same rules.
    // That is, a target left of a source, or a Westbound move, opposite of the above Eastbound move,
    // should be "walked" left to right.
    // (target origin X is < source origin X)
    //
    // We haven't given the sample yet that Northbound and Southbound moves are the same, but we
    // could reason that the same logic applies and the conclusion would be a Northbound move
    // would walk from the target toward the source again... a.k.a. Top to Bottom.
    // (target origin Y is < source origin Y)
    // Then the Southbound move would be the opposite, Bottom to Top.
    // (target origin Y is > source origin Y)
    //
    // To confirm, let's try one more example but moving both at once in an ordinal direction Northeast.
    //
    //                 target
    //                 origin 1, 0
    //                 |
    //                 v
    //                 +----target--                         +----target--
    //  source      A  |  B     C  |                      A  |  D     E  |
    //  origin-->+------------     |     becomes       +------------     |
    //   0, 1    |  D  |  E  |  F  |    =========>     |  D  |  G  |  H  |
    //           |     -------------                   |     -------------
    //           |  G     H  |  I                      |  G     H  |  I
    //           --source-----                         --source-----
    //
    // Following our supposed rules from above, we have...
    // Source Origin X = 0, Y = 1
    // Target Origin X = 1, Y = 0
    //
    // Source Origin X < Target Origin X which means Right to Left
    // Source Origin Y > Target Origin Y which means Top to Bottom
    //
    // So the first thing we should copy is the Top and Right most
    // value from source to target.
    //
    //        +----target--                         +----target--
    //     A  |  B     C  |                      A  |  B     E  |
    //  +------------     |     step 1        +------------     |
    //  |  D  |  E  |  F  |    =========>     |  D  |  E  |  F  |
    //  |     -------------                   |     -------------
    //  |  G     H  |  I                      |  G     H  |  I
    //  --source-----                         --source-----
    //
    // And look. The E which was in the overlapping part of the source
    // is the first thing copied out of the way and we're safe to copy the rest.
    //
    // We assume that this pattern then applies to all ordinal directions as well
    // and it appears our rules hold.
    //
    // We've covered all cardinal and ordinal directions... all that is left is two
    // rectangles of the same size and origin... and in that case, it doesn't matter
    // as nothing is moving and therefore can't be covered up or lost.
    //
    // Therefore, we will codify our inequalities below as determining the walk direction
    // for a given source and target viewport and use the helper `GetWalkOrigin`
    // to return the place that we should start walking from when the copy commences.

    const auto sourceOrigin = source.Origin();
    const auto targetOrigin = target.Origin();

    return Viewport::WalkDir{ targetOrigin.x < sourceOrigin.x ? Viewport::XWalk::LeftToRight : Viewport::XWalk::RightToLeft,
                              targetOrigin.y < sourceOrigin.y ? Viewport::YWalk::TopToBottom : Viewport::YWalk::BottomToTop };
}

// Method Description:
// - Clips the input rectangle to our bounds. Assumes that the input rectangle
//is an exclusive rectangle.
// Arguments:
// - psr: a pointer to an exclusive rect to clip.
// Return Value:
// - true iff the clipped rectangle is valid (with a width and height both >0)
bool Viewport::TrimToViewport(_Inout_ til::rect* psr) const noexcept
{
    psr->left = std::max(psr->left, Left());
    psr->right = std::min(psr->right, RightExclusive());
    psr->top = std::max(psr->top, Top());
    psr->bottom = std::min(psr->bottom, BottomExclusive());

    return psr->left < psr->right && psr->top < psr->bottom;
}

// Method Description:
// - Translates the input til::rect out of our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - psr: a pointer to a til::rect the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertToOrigin(_Inout_ til::rect* psr) const noexcept
{
    const auto dx = Left();
    const auto dy = Top();
    psr->left -= dx;
    psr->right -= dx;
    psr->top -= dy;
    psr->bottom -= dy;
}

// Method Description:
// - Translates the input til::inclusive_rect out of our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - psr: a pointer to a til::inclusive_rect the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertToOrigin(_Inout_ til::inclusive_rect* const psr) const noexcept
{
    const auto dx = Left();
    const auto dy = Top();
    psr->left -= dx;
    psr->right -= dx;
    psr->top -= dy;
    psr->bottom -= dy;
}

// Method Description:
// - Translates the input coordinate out of our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - pcoord: a pointer to a coordinate the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertToOrigin(_Inout_ til::point* const pcoord) const noexcept
{
    pcoord->x -= Left();
    pcoord->y -= Top();
}

// Method Description:
// - Translates the input til::inclusive_rect to our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - psr: a pointer to a til::inclusive_rect the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertFromOrigin(_Inout_ til::inclusive_rect* const psr) const noexcept
{
    const auto dx = Left();
    const auto dy = Top();
    psr->left += dx;
    psr->right += dx;
    psr->top += dy;
    psr->bottom += dy;
}

// Method Description:
// - Translates the input coordinate to our coordinate space, whose origin is
//      at (this.left, this.right)
// Arguments:
// - pcoord: a pointer to a coordinate the translate into our coordinate space.
// Return Value:
// - <none>
void Viewport::ConvertFromOrigin(_Inout_ til::point* const pcoord) const noexcept
{
    pcoord->x += Left();
    pcoord->y += Top();
}

// Method Description:
// - Returns an exclusive til::rect equivalent to this viewport.
// Arguments:
// - <none>
// Return Value:
// - an exclusive til::rect equivalent to this viewport.
til::rect Viewport::ToExclusive() const noexcept
{
    return { Left(), Top(), RightExclusive(), BottomExclusive() };
}

// Method Description:
// - Returns an inclusive til::inclusive_rect equivalent to this viewport.
// Arguments:
// - <none>
// Return Value:
// - an inclusive til::inclusive_rect equivalent to this viewport.
til::inclusive_rect Viewport::ToInclusive() const noexcept
{
    return { Left(), Top(), RightInclusive(), BottomInclusive() };
}

// Method Description:
// - Returns a new viewport representing this viewport at the origin.
// For example:
//  this = {6, 5, 11, 11} (w, h = 5, 6)
//  result = {0, 0, 5, 6} (w, h = 5, 6)
// Arguments:
// - <none>
// Return Value:
// - a new viewport with the same dimensions as this viewport with top, left = 0, 0
Viewport Viewport::ToOrigin() const noexcept
{
    auto returnVal = *this;
    ConvertToOrigin(&returnVal._sr);
    return returnVal;
}

// Method Description:
// - Translates another viewport to this viewport's coordinate space.
// For example:
//  this = {5, 6, 7, 8} (w,h = 1, 1)
//  other = {6, 5, 11, 11} (w, h = 5, 6)
//  result = {1, -1, 6, 5} (w, h = 5, 6)
// Arguments:
// - other: the viewport to convert to this coordinate space
// Return Value:
// - the input viewport in a the coordinate space with origin at (this.top, this.left)
[[nodiscard]] Viewport Viewport::ConvertToOrigin(const Viewport& other) const noexcept
{
    auto returnVal = other;
    ConvertToOrigin(&returnVal._sr);
    return returnVal;
}

// Method Description:
// - Translates another viewport out of this viewport's coordinate space.
// For example:
//  this = {5, 6, 7, 8} (w,h = 1, 1)
//  other = {0, 0, 5, 6} (w, h = 5, 6)
//  result = {5, 6, 10, 12} (w, h = 5, 6)
// Arguments:
// - other: the viewport to convert out of this coordinate space
// Return Value:
// - the input viewport in a the coordinate space with origin at (0, 0)
[[nodiscard]] Viewport Viewport::ConvertFromOrigin(const Viewport& other) const noexcept
{
    auto returnVal = other;
    ConvertFromOrigin(&returnVal._sr);
    return returnVal;
}

// Function Description:
// - Translates a given Viewport by the specified coord amount. Does the
//      addition with safemath.
// Arguments:
// - original: The initial viewport to translate. Is unmodified by this operation.
// - delta: The amount to translate the original rect by, in both the x and y coordinates.
// Return Value:
// - The offset viewport by the given delta.
// - NOTE: Throws on safe math failure.
[[nodiscard]] Viewport Viewport::Offset(const Viewport& original, const til::point delta) noexcept
{
    const auto newLeft = original._sr.left + delta.x;
    const auto newTop = original._sr.top + delta.y;
    const auto newRight = original._sr.right + delta.x;
    const auto newBottom = original._sr.bottom + delta.y;
    return Viewport({ newLeft, newTop, newRight, newBottom });
}

// Function Description:
// - Returns a viewport created from the union of both the parameter viewports.
//      The result extends from the leftmost extent of either rect to the
//      rightmost extent of either rect, and from the lowest top value to the
//      highest bottom value, and everything in between.
// Arguments:
// - lhs: one of the viewports to or together
// - rhs: the other viewport to or together
// Return Value:
// - a Viewport representing the union of the other two viewports.
[[nodiscard]] Viewport Viewport::Union(const Viewport& lhs, const Viewport& rhs) noexcept
{
    const auto leftValid = lhs.IsValid();
    const auto rightValid = rhs.IsValid();

    // If neither are valid, return empty.
    if (!leftValid && !rightValid)
    {
        return Viewport::Empty();
    }
    // If left isn't valid, then return just the right.
    else if (!leftValid)
    {
        return rhs;
    }
    // If right isn't valid, then return just the left.
    else if (!rightValid)
    {
        return lhs;
    }
    // Otherwise, everything is valid. Find the actual union.
    else
    {
        const auto left = std::min(lhs.Left(), rhs.Left());
        const auto top = std::min(lhs.Top(), rhs.Top());
        const auto right = std::max(lhs.RightInclusive(), rhs.RightInclusive());
        const auto bottom = std::max(lhs.BottomInclusive(), rhs.BottomInclusive());

        return Viewport({ left, top, right, bottom });
    }
}

// Function Description:
// - Creates a viewport from the intersection of both the parameter viewports.
//      The result will be the smallest area that fits within both rectangles.
// Arguments:
// - lhs: one of the viewports to intersect
// - rhs: the other viewport to intersect
// Return Value:
// - a Viewport representing the intersection of the other two, or an empty viewport if there's no intersection.
[[nodiscard]] Viewport Viewport::Intersect(const Viewport& lhs, const Viewport& rhs) noexcept
{
    const auto left = std::max(lhs.Left(), rhs.Left());
    const auto top = std::max(lhs.Top(), rhs.Top());
    const auto right = std::min(lhs.RightInclusive(), rhs.RightInclusive());
    const auto bottom = std::min(lhs.BottomInclusive(), rhs.BottomInclusive());

    const Viewport intersection({ left, top, right, bottom });

    // What we calculated with min/max might not actually represent a valid viewport that has area.
    // If we calculated something that is nonsense (invalid), then just return the empty viewport.
    if (!intersection.IsValid())
    {
        return Viewport::Empty();
    }
    else
    {
        // If it was valid, give back whatever we created.
        return intersection;
    }
}

// Routine Description:
// - Returns a list of Viewports representing the area from the `original` Viewport that was NOT a part of
//   the given `removeMe` Viewport. It can require multiple Viewports to represent the remaining
//   area as a "region".
// Arguments:
// - original - The overall viewport to start from.
// - removeMe - The space that should be taken out of the main Viewport.
// Return Value:
// - Array of 4 Viewports representing non-overlapping segments of the remaining area
//   that was covered by `main` before the regional area of `removeMe` was taken out.
// - You must check that each viewport .IsValid() before using it.
[[nodiscard]] SomeViewports Viewport::Subtract(const Viewport& original, const Viewport& removeMe) noexcept
try
{
    SomeViewports result;

    // We could have up to four rectangles describing the area resulting when you take removeMe out of main.
    // Find the intersection of the two so we know which bits of removeMe are actually applicable
    // to the original rectangle for subtraction purposes.
    const auto intersection = Viewport::Intersect(original, removeMe);

    // If there's no intersection, there's nothing to remove.
    if (!intersection.IsValid())
    {
        // Just put the original rectangle into the results and return early.
        result.push_back(original);
    }
    // If the original rectangle matches the intersection, there is nothing to return.
    else if (original != intersection)
    {
        // Generate our potential four viewports that represent the region of the original that falls outside of the remove area.
        // We will bias toward generating wide rectangles over tall rectangles (if possible) so that optimizations that apply
        // to manipulating an entire row at once can be realized by other parts of the console code. (i.e. Run Length Encoding)
        // In the following examples, the found remaining regions are represented by:
        // T = Top      B = Bottom      L = Left        R = Right
        //
        // 4 Sides but Identical:
        // |---------original---------|             |---------original---------|
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |    ======>  |        intersect         |  ======>  early return of nothing
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |---------removeMe---------|             |--------------------------|
        //
        // 4 Sides:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |        |---------|       |             |        |---------|       |           |LLLLLLLL|---------|RRRRRRR|
        // |        |removeMe |       |    ======>  |        |intersect|       |  ======>  |LLLLLLLL|         |RRRRRRR|
        // |        |---------|       |             |        |---------|       |           |LLLLLLLL|---------|RRRRRRR|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |--------------------------|             |--------------------------|           |--------------------------|
        //
        // 3 Sides:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
        // |        |removeMe            | ======>  |        |intersect        |  ======>  |LLLLLLLL|                 |
        // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |                          |             |                          |           |BBBBBBBBBBBBBBBBBBBBBBBBBB|
        // |--------------------------|             |--------------------------|           |--------------------------|
        //
        // 2 Sides:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |        |--------------------|          |        |-----------------|           |LLLLLLLL|-----------------|
        // |        |removeMe            | ======>  |        |intersect        |  ======>  |LLLLLLLL|                 |
        // |        |                    |          |        |                 |           |LLLLLLLL|                 |
        // |        |                    |          |        |                 |           |LLLLLLLL|                 |
        // |        |                    |          |        |                 |           |LLLLLLLL|                 |
        // |--------|                    |          |--------------------------|           |--------------------------|
        //          |                    |
        //          |--------------------|
        //
        // 1 Side:
        // |---------original---------|             |---------original---------|           |--------------------------|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |                          |             |                          |           |TTTTTTTTTTTTTTTTTTTTTTTTTT|
        // |-----------------------------|          |--------------------------|           |--------------------------|
        // |         removeMe            | ======>  |         intersect        |  ======>  |                          |
        // |                             |          |                          |           |                          |
        // |                             |          |                          |           |                          |
        // |                             |          |                          |           |                          |
        // |                             |          |--------------------------|           |--------------------------|
        // |                             |
        // |-----------------------------|
        //
        // 0 Sides:
        // |---------original---------|             |---------original---------|
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |    ======>  |                          |  ======>  early return of Original
        // |                          |             |                          |
        // |                          |             |                          |
        // |                          |             |                          |
        // |--------------------------|             |--------------------------|
        //
        //
        //         |---------------|
        //         | removeMe      |
        //         |---------------|

        // We generate these rectangles by the original and intersection points, but some of them might be empty when the intersection
        // lines up with the edge of the original. That's OK. That just means that the subtraction didn't leave anything behind.
        // We will filter those out below when adding them to the result.
        const Viewport top{ til::inclusive_rect{ original.Left(), original.Top(), original.RightInclusive(), intersection.Top() - 1 } };
        const Viewport bottom{ til::inclusive_rect{ original.Left(), intersection.BottomExclusive(), original.RightInclusive(), original.BottomInclusive() } };
        const Viewport left{ til::inclusive_rect{ original.Left(), intersection.Top(), intersection.Left() - 1, intersection.BottomInclusive() } };
        const Viewport right{ til::inclusive_rect{ intersection.RightExclusive(), intersection.Top(), original.RightInclusive(), intersection.BottomInclusive() } };

        if (top.IsValid())
        {
            result.push_back(top);
        }

        if (bottom.IsValid())
        {
            result.push_back(bottom);
        }

        if (left.IsValid())
        {
            result.push_back(left);
        }

        if (right.IsValid())
        {
            result.push_back(right);
        }
    }

    return result;
}
CATCH_FAIL_FAST()

// Method Description:
// - Returns true if the rectangle described by this Viewport has internal space
// - i.e. it has a positive, non-zero height and width.
bool Viewport::IsValid() const noexcept
{
    return static_cast<bool>(_sr);
}
