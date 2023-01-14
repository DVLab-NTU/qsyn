/****************************************************************************
  FileName     [ cmdMacros.h ]
  PackageName  [ cmd ]
  Synopsis     [ Define macros for command parsing ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef CMD_MACROS_H
#define CMD_MACROS_H

#define CMD_N_OPTS_AT_LEAST_OR_RETURN(options, lower)                                       \
    {                                                                                       \
        if ((options).size() < (lower)) {                                                   \
            return errorOption(CMD_OPT_MISSING, (options).empty() ? "" : (options).back()); \
        }                                                                                   \
    }

#define CMD_N_OPTS_AT_MOST_OR_RETURN(options, upper)               \
    {                                                              \
        if ((options).size() > (upper)) {                          \
            return errorOption(CMD_OPT_EXTRA, (options)[(upper)]); \
        }                                                          \
    }

#define CMD_N_OPTS_BETWEEN_OR_RETURN(options, lower, upper) \
    {                                                       \
        CMD_N_OPTS_AT_LEAST_OR_RETURN((options), (lower));  \
        CMD_N_OPTS_AT_MOST_OR_RETURN((options), (upper));   \
    }

#define CMD_N_OPTS_EQUAL_OR_RETURN(options, num)              \
    {                                                         \
        CMD_N_OPTS_BETWEEN_OR_RETURN((options), (num), (num)) \
    }

#endif  // CMD_MACROS_H