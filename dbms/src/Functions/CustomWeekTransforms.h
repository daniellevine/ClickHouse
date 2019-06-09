#pragma once
#include <regex>
#include <Columns/ColumnVector.h>
#include <Columns/ColumnsNumber.h>
#include <Core/Types.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/extractTimeZoneFromFunctionArguments.h>
#include <Common/Exception.h>
#include <common/DateLUTImpl.h>

/// The default mode value to use for the WEEK() function
#define DEFAULT_WEEK_MODE 0

namespace DB
{
namespace ErrorCodes
{
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int ILLEGAL_COLUMN;
}

/**
 * CustomWeek Transformations.
  */

static inline UInt32 dateIsNotSupported(const char * name)
{
    throw Exception("Illegal type Date of argument for function " + std::string(name), ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT);
}

/// This factor transformation will say that the function is monotone everywhere.
struct ZeroTransform
{
    static inline UInt16 execute(UInt32, UInt8, const DateLUTImpl &) { return 0; }
    static inline UInt16 execute(UInt16, UInt8, const DateLUTImpl &) { return 0; }
};

struct WeekImpl
{
    static constexpr auto name = "week";

    static inline UInt8 execute(UInt32 t, UInt8 week_mode, const DateLUTImpl & time_zone)
    {
        UInt32 year = 0;
        return time_zone.calc_week(time_zone.toDayNum(t), week_mode, &year);
    }
    static inline UInt8 execute(UInt16 d, UInt8 week_mode, const DateLUTImpl & time_zone)
    {
        UInt32 year = 0;
        return time_zone.calc_week(DayNum(d), week_mode, &year);
    }

    using FactorTransform = ZeroTransform;
};

struct YearWeekImpl
{
    static constexpr auto name = "yearWeek";

    static inline UInt32 execute(UInt32 t, UInt8 week_mode, const DateLUTImpl & time_zone)
    {
        return time_zone.calc_yearWeek(time_zone.toDayNum(t), week_mode);
    }
    static inline UInt32 execute(UInt16 d, UInt8 week_mode, const DateLUTImpl & time_zone)
    {
        return time_zone.calc_yearWeek(DayNum(d), week_mode);
    }

    using FactorTransform = ZeroTransform;
};

template <typename FromType, typename ToType, typename Transform>
struct Transformer
{
    static void
    vector(const PaddedPODArray<FromType> & vec_from, PaddedPODArray<ToType> & vec_to, UInt8 week_mode, const DateLUTImpl & time_zone)
    {
        size_t size = vec_from.size();
        vec_to.resize(size);

        for (size_t i = 0; i < size; ++i)
            vec_to[i] = Transform::execute(vec_from[i], week_mode, time_zone);
    }
};


template <typename FromType, typename ToType, typename Transform>
struct CustomWeekTransformImpl
{
    static void execute(Block & block, const ColumnNumbers & arguments, size_t result, size_t /*input_rows_count*/)
    {
        using Op = Transformer<FromType, ToType, Transform>;

        UInt8 week_mode = DEFAULT_WEEK_MODE;
        if (arguments.size() > 1)
        {
            if (const auto week_mode_column = checkAndGetColumnConst<ColumnUInt8>(block.getByPosition(arguments[1]).column.get()))
                week_mode = week_mode_column->getValue<UInt8>();
        }

        const DateLUTImpl & time_zone = extractTimeZoneFromFunctionArguments(block, arguments, 2, 0);
        const ColumnPtr source_col = block.getByPosition(arguments[0]).column;
        if (const auto * sources = checkAndGetColumn<ColumnVector<FromType>>(source_col.get()))
        {
            auto col_to = ColumnVector<ToType>::create();
            Op::vector(sources->getData(), col_to->getData(), week_mode, time_zone);
            block.getByPosition(result).column = std::move(col_to);
        }
        else
        {
            throw Exception(
                "Illegal column " + block.getByPosition(arguments[0]).column->getName() + " of first argument of function "
                    + Transform::name,
                ErrorCodes::ILLEGAL_COLUMN);
        }
    }
};

}
