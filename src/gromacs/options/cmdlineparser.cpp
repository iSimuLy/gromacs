/*
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2009, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 *
 * For more info, check our website at http://www.gromacs.org
 */
/*! \internal \file
 * \brief
 * Implements gmx::CommandLineParser.
 *
 * \author Teemu Murtola <teemu.murtola@cbr.su.se>
 * \ingroup module_options
 */
#include "gromacs/options/cmdlineparser.h"

#include "gromacs/fatalerror/exceptions.h"
#include "gromacs/fatalerror/messagestringcollector.h"
#include "gromacs/options/optionsassigner.h"

#include "cmdlineparser-impl.h"

namespace gmx
{

/********************************************************************
 * CommandLineParser::Impl
 */

CommandLineParser::Impl::Impl(Options *options)
    : _assigner(options)
{
    _assigner.setAcceptBooleanNoPrefix(true);
    _assigner.setNoStrictSectioning(true);
}

/********************************************************************
 * CommandLineParser
 */

CommandLineParser::CommandLineParser(Options *options)
    : _impl(new Impl(options))
{
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::parse(int *argc, char *argv[])
{
    MessageStringCollector errors;
    int  i = 1;
    // Start in the discard phase to skip options that can't be understood.
    bool bDiscard = true;

    _impl->_assigner.start();
    while (i < *argc)
    {
        // Lone '-' is passed as a value.
        if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            if (!bDiscard)
            {
                try
                {
                    _impl->_assigner.finishOption();
                }
                catch (const UserInputError &ex)
                {
                    errors.append(ex.what());
                }
                errors.finishContext();
            }
            errors.startContext("In command-line option " + std::string(argv[i]));
            bDiscard = false;
            try
            {
                const char *name = &argv[i][1];
                _impl->_assigner.startOption(name);
            }
            catch (const UserInputError &ex)
            {
                bDiscard = true;
                errors.append(ex.what());
                errors.finishContext();
            }
        }
        else if (!bDiscard)
        {
            try
            {
                _impl->_assigner.appendValue(argv[i]);
            }
            catch (const UserInputError &ex)
            {
                errors.append(ex.what());
            }
        }
        ++i;
    }
    if (!bDiscard)
    {
        try
        {
            _impl->_assigner.finishOption();
        }
        catch (const UserInputError &ex)
        {
            errors.append(ex.what());
        }
        errors.finishContext();
    }
    _impl->_assigner.finish();
    if (!errors.isEmpty())
    {
        // TODO: This exception type may not always be appropriate.
        GMX_THROW(InvalidInputError(errors.toString()));
    }
}

} // namespace gmx
