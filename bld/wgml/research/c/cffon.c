/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Implements the functions declared in cffon.h:
*                   is_fon_file()
*                   parse_font()
*               and this local function:
*                   resize_cop_font()
*
****************************************************************************/

#define __STDC_WANT_LIB_EXT1__ 1
//#include <stddef.h>
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cffon.h"

/*  Local macros */

#define START_SIZE 2048
#define INC_SIZE   1024

/*  Local function declaration */

static cop_font * resize_cop_font( cop_font *, size_t );

/*  Function is_fon_file().
 *  Determines whether or not in_file points to the start of a .COP font
 *  file (the first byte after the header).
 *
 *  Parameter:
 *      in_file points to the presumed start of a .COP font file
 *
 *  Returns:
 *      true if this has the correct designator
 *      false otherwise
 *
 */

bool is_fon_file( FILE * in_file)
{
    char designator[3];

    /* Get the designator */

    fread( &designator, 3, 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) return( false );

    /* Verify that the designator is for a .COP font file */

    if( memcmp( designator, "FON", 3 ) ) return( false );
    
    return( true );
}

/*  Function parse_font().
 *  Constructs a cop_font instance from the given input stream.
 *  
 *  Parameters:
 *      in_file points to the first byte of a .COP file encoding a :FONT
 *          block after the "FON" descriminator.
 *
 *  Returns:
 *      A pointer to a cop_font struct containing the data from in_file
 *          on success.
 *      A NULL pointer on failure.
 */

cop_font * parse_font( FILE * in_file )
{

    /* The out_font instance */
  
    cop_font *          out_font          = NULL;

    /* Used to acquire string attributes */

    uint8_t             length;
    char *              string_ptr          = NULL;

    /* Used to acquire numeric attributes */

    uint16_t            designator;
    uint16_t            numeric_16;

    /* Used to acquire the CharacterDescriptionBlock */

    uint8_t *           byte_ptr            = NULL;
    uint8_t             data_count;
    uint16_t            i;
    uint8_t             intrans_flag;
    uint8_t *           outtrans_data       = NULL;
    uint8_t             outtrans_data_size;
    uint8_t             outtrans_flag;
    outtrans_block *    outtrans_ptr        = NULL;
    translation *       translation_ptr     = NULL;
    uint8_t *           translation_start   = NULL;
    uint8_t             uint8_array[0x100];
    uint16_t            uint16_array[0x100];    
    uint8_t             width_data_size;
    uint8_t             width_flag;
    uint32_t *          width_ptr;

    /* Used for count and other values */

    uint8_t             count8;

    /* Initialize the out_font instance */
        
    out_font = (cop_font *) malloc( START_SIZE );
    if( out_font == NULL ) return( out_font );

    out_font->allocated_size = START_SIZE;
    out_font->next_offset = sizeof( cop_font );

    /* Note: The various pointers must be entered, initially, as offsets
     * and then converted to pointers before returning because out_font
     * may be reallocated at any point and that invalidates actual pointers.
     */

    /* Get the first font output name */

    fread( &length, sizeof( length ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }

    if( length > 0 ) {

        if( out_font->allocated_size < (out_font->next_offset + length) ) {
            out_font = resize_cop_font( out_font, length );
            if( out_font == NULL ) return( out_font );
        }

        string_ptr = (char *) out_font + out_font->next_offset;

        fread( string_ptr, length, 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }
        out_font->font_out_name1 = (char *) out_font->next_offset;
        out_font->next_offset += length;
        string_ptr[length] = 0x00;
        ++out_font->next_offset;
    } else {
        out_font->font_out_name1 = NULL;
    }

    /* Get the second font output name */

    fread( &length, sizeof( length ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }

    if( length > 0 ) {
        if( out_font->allocated_size < (out_font->next_offset + length) ) {
            out_font = resize_cop_font( out_font, length );
            if( out_font == NULL ) return( out_font );
        }

        string_ptr = (char *) out_font + out_font->next_offset;

        fread( string_ptr, length, 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }
    
        out_font->font_out_name2 = (char *) out_font->next_offset;
        out_font->next_offset += length;
        string_ptr[length] = 0x00;
        ++out_font->next_offset;
    } else {
        out_font->font_out_name2 = NULL;
    }

    /* Get the numeric attributes */

    /* The designator shows if some of the numeric attributes are uint16_t
     * or uint32_t (the other numeric attributes are always 16-bit)
     */

    fread( &designator, sizeof( designator ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }
    
    switch( designator ) {
    case 0x1700:

        /* Get the 16-bit Line Height */

        fread( &numeric_16, sizeof(numeric_16), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        out_font->line_height = numeric_16;

        /* Get the 16-bit Line Space */

        fread( &numeric_16, sizeof( numeric_16 ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        out_font->line_space = numeric_16;

        /* Get the 32-bit Scale Basis */

        fread( &out_font->scale_basis, sizeof( out_font->scale_basis ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Scale Minimum */

        fread( &out_font->scale_min, sizeof( out_font->scale_min ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Scale Maximum */

        fread( &out_font->scale_max, sizeof( out_font->scale_max ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 16-bit Character Width */

        fread( &numeric_16, sizeof( numeric_16 ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        out_font->char_width = numeric_16;

        break;
    case 0x1D00:

        /* Get the 32-bit Line Height */

        fread( &out_font->line_height, sizeof( out_font->line_height), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Line Space */

        fread( &out_font->line_space, sizeof( out_font->line_space ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Scale Basis */

        fread( &out_font->scale_basis, sizeof( out_font->scale_basis ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Scale Minimum */

        fread( &out_font->scale_min, sizeof( out_font->scale_min ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Scale Maximum */

        fread( &out_font->scale_max, sizeof( out_font->scale_max ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* Get the 32-bit Character Width */

        fread( &out_font->char_width, sizeof( out_font->char_width ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        break;
    default:
        printf_s( "Bad numeric parameters designator: %i\n", designator );
        free( out_font );
        out_font = NULL;
        return( out_font );
    }

    /* Get the CharacterDescriptionBlock */

    /* This value indicates WidthBlock data size, but it is not needed */

    fread( &count8, sizeof( count8 ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }
  
    /* Get the data_count and the flags */

    fread( &width_flag, sizeof( width_flag ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }
  
    fread( &data_count, sizeof( data_count ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }

    fread( &outtrans_flag, sizeof( outtrans_flag ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }

    fread( &intrans_flag, sizeof( intrans_flag ), 1, in_file );
    if( ferror( in_file ) || feof( in_file ) ) {
        free( out_font );
        out_font = NULL;
        return( out_font );
    }

    /* Get the IntransBlock, if present */

    if(intrans_flag == 0) {
        out_font->intrans = NULL;
    } else {

        /* Verify that the next byte is 0x81 */

        fread( &count8, sizeof( count8 ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
           free( out_font );
           out_font = NULL;
           return( out_font );
        }

        if( count8 != 0x81 ) {
           printf_s( "Incorrect IntransBlock designator: %i\n", count8 );
           free( out_font );
           out_font = NULL;
           return( out_font );
        }

        /* Get the count byte and verify that it contains 0x00 */
      
        fread( &count8, sizeof( count8 ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
           free( out_font );
           out_font = NULL;
           return( out_font );
        }

        if( count8 != 0x00 ) {
            printf_s( "Incorrect IntransBlock count: %i\n", count8 );
            free( out_font );
            out_font = NULL;
            return( out_font );
        }
  
        /* Get the data into the array */

        if( out_font->allocated_size < (out_font->next_offset + sizeof( out_font->intrans->table )) ) {
            out_font = resize_cop_font( out_font, sizeof( out_font->intrans->table ) );
            if( out_font == NULL ) return( out_font );
        }

        byte_ptr = (uint8_t *) out_font + out_font->next_offset;
        
        fread( byte_ptr, sizeof( out_font->intrans->table ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
           free( out_font );
           out_font = NULL;
           return( out_font );
        }

        out_font->intrans = (intrans_block *) out_font->next_offset;
        out_font->next_offset += sizeof( out_font->intrans->table );
    }  

    /* Get the OuttransBlock, if present */

    if(outtrans_flag == 0) {
        out_font->outtrans = NULL;
    } else {

        /* Get the next byte, which indicates the OuttransBlock data size */

        fread( &outtrans_data_size, sizeof( outtrans_data_size ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
           free( out_font );
           out_font = NULL;
           return( out_font );
        }

        /* Read the count byte */

        fread( &count8, sizeof( count8 ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* The file is positioned at the start of the data */

        switch( outtrans_data_size) {
        case 0x81:

            /* The count byte should be 0x00 */

            if( count8 != 0x00 ) {
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Get the data into the local buffer */

            fread( &uint8_array, sizeof( uint8_array ), 1, in_file );
            if( ferror( in_file ) || feof( in_file ) ) {
                free( out_font );
                out_font = NULL;
                return( out_font );
            }
            
            /* Reserve space for the outtrans_block */

            if( out_font->allocated_size < (out_font->next_offset + sizeof( out_font->outtrans->table )) ) {
                out_font = resize_cop_font( out_font, sizeof( out_font->outtrans->table ) );
                if( out_font == NULL ) {
                    free( outtrans_data );
                    outtrans_data = NULL;
                    return( out_font );
                }
            }

            out_font->outtrans = (outtrans_block *) out_font->next_offset;
            out_font->next_offset += sizeof( out_font->outtrans->table );

            outtrans_ptr = (outtrans_block *) ((char *) out_font + (size_t) out_font->outtrans);

            /* Build the actual table, which requires actual pointers in
             * place of the offsets recorded in *out_font:
             *   outtrans_ptr is the pointer version of out_font->outtrans
             *   for each iteration,
             *     translation_ptr is the pointer version of outtrans->ptr->table[i]
             *     byte_ptr is the pointer version of translation->ptr.data
             */

            for( i = 0; i < 0x100; i++ ) {

                /* If the first byte matches the index, there is no translation */

                if( uint8_array[i] == i) {
                    outtrans_ptr->table[i] = NULL;
                } else {

                    /* Reserve space for the translation */

                    if( out_font->allocated_size < (out_font->next_offset + sizeof( translation )) ) {
                        out_font = resize_cop_font( out_font, sizeof( translation ) );
                        if( out_font == NULL ) {
                            free( outtrans_data );
                            outtrans_data = NULL;
                            return( out_font );
                        }
                        outtrans_ptr = (outtrans_block *) ((uint8_t *) out_font + (size_t) out_font->outtrans);
                    }

                    outtrans_ptr->table[i] = (translation *) out_font->next_offset;
                    out_font->next_offset += sizeof( translation );

                    /* Get the translation for the current character */

                    translation_ptr = (translation *) ((char *) out_font + (size_t) outtrans_ptr->table[i] );

                    /* The translation always contains exactly one character */
                    
                    translation_ptr->count = 1;

                    if( out_font->allocated_size < (out_font->next_offset + translation_ptr->count ) ) {
                        out_font = resize_cop_font( out_font, translation_ptr->count  );
                        if( out_font == NULL ) {
                            free( outtrans_data );
                            outtrans_data = NULL;
                            return( out_font );
                        }
                        outtrans_ptr = (outtrans_block *) ((uint8_t *) out_font + (size_t) out_font->outtrans);
                        translation_ptr = (translation *) ((uint8_t *) out_font + (size_t) outtrans_ptr->table[i] );
                    }

                    translation_ptr->data = (uint8_t *) out_font->next_offset;
                    out_font->next_offset += translation_ptr->count;

                    byte_ptr = (uint8_t *) out_font + (size_t) translation_ptr->data;

                    /* The translation character is the value in the input array */
                    
                    *byte_ptr = uint8_array[i];
                }
            }
            break;
        case 0x82:

            /* The count byte should be equal to data_count */
        
            if( count8 != data_count ) {
                printf_s( "Incorrect OuttransBlock data_count: %i instead of %i\n", data_count, count8 );
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Get the outtrans array into the local array */

            fread( &uint16_array, sizeof( uint16_array ), 1, in_file );
            if( ferror( in_file ) || feof( in_file ) ) {
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Allocate a buffer and read the translation characters into it */

            outtrans_data = (uint8_t *) malloc( data_count );

            fread( outtrans_data, sizeof( *outtrans_data ), data_count, in_file );
            if( ferror( in_file ) || feof( in_file ) ) {
                free( outtrans_data );
                outtrans_data = NULL;
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Initialize outtrans_ptr and the outtrans pointer in out_font */

            if( out_font->allocated_size < (out_font->next_offset + sizeof( out_font->outtrans->table )) ) {
                out_font = resize_cop_font( out_font, sizeof( out_font->outtrans->table ) );
                if( out_font == NULL ) {
                    free( outtrans_data );
                    outtrans_data = NULL;
                    return( out_font );
                }
            }

            out_font->outtrans = (outtrans_block *) out_font->next_offset;
            out_font->next_offset += sizeof( out_font->outtrans->table );

            outtrans_ptr = (outtrans_block *) ((char *) out_font + (size_t) out_font->outtrans);

            /* Convert the data in uint16_array to our format, which requires
             * actual pointers in place of the offsets recorded in *out_font:
             *   outtrans_ptr is the pointer version of out_font->outtrans
             *   for each iteration,
             *     translation_ptr is the pointer version of outtrans->ptr->table[i]
             *     byte_ptr is the pointer version of translation->ptr.data
             */

            for( i = 0; i < 0x100; i++ ) {

                /* If the first byte matches the index, there is no translation */

                if( uint16_array[i] == i) {
                    outtrans_ptr->table[i] = NULL;
                } else {

                    /* Reserve space for the translation */

                    if( out_font->allocated_size < (out_font->next_offset + sizeof( translation )) ) {
                        out_font = resize_cop_font( out_font, sizeof( translation ) );
                        if( out_font == NULL ) {
                            free( outtrans_data );
                            outtrans_data = NULL;
                            return( out_font );
                        }
                        outtrans_ptr = (outtrans_block *) ((uint8_t *) out_font + (size_t) out_font->outtrans);
                    }

                    outtrans_ptr->table[i] = (translation *) out_font->next_offset;
                    out_font->next_offset += sizeof( translation );

                    /* Get the translation for the current character */

                    translation_ptr = (translation *) ((char *) out_font + (size_t) outtrans_ptr->table[i] );

                    translation_start = outtrans_data + (uint16_array[i] & 0x00ff);        
                    translation_ptr->count = *translation_start;
                    ++translation_start;

                    if( out_font->allocated_size < (out_font->next_offset + translation_ptr->count ) ) {
                        out_font = resize_cop_font( out_font, translation_ptr->count  );
                        if( out_font == NULL ) {
                            free( outtrans_data );
                            outtrans_data = NULL;
                            return( out_font );
                        }
                        outtrans_ptr = (outtrans_block *) ((uint8_t *) out_font + (size_t) out_font->outtrans);
                        translation_ptr = (translation *) ((uint8_t *) out_font + (size_t) outtrans_ptr->table[i] );
                    }

                    translation_ptr->data = (uint8_t *) out_font->next_offset;
                    out_font->next_offset += translation_ptr->count;

                    byte_ptr = (uint8_t *) out_font + (size_t) translation_ptr->data;
                    memcpy_s(byte_ptr, translation_ptr->count, translation_start, translation_ptr->count );
                }
            }
            break;
        default:
           printf_s( "Incorrect OuttransBlock designator: %i\n", count8 );
           free( out_font );
           out_font = NULL;
           return( out_font );
        }
        free( outtrans_data );
        outtrans_data = NULL;
    }  

    /* Get the WidthBlock, if present */

    if(width_flag != 0) {
        out_font->width = NULL;
    } else {

        /* Reserve the space for width */

        if( out_font->allocated_size < (out_font->next_offset + sizeof( out_font->width->table )) ) {
            out_font = resize_cop_font( out_font, sizeof( out_font->width->table ) );
            if( out_font == NULL ) return( out_font );
        }

        width_ptr = (uint32_t *) ((uint8_t *) out_font + out_font->next_offset);
        
        out_font->width = (width_block *) out_font->next_offset;
        out_font->next_offset += sizeof( out_font->width->table );

        /* Get the next byte, which indicates the WidthBlock data size */

        fread( &width_data_size, sizeof( width_data_size ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* This byte's value is checked below */

        fread( &count8, sizeof( count8 ), 1, in_file );
        if( ferror( in_file ) || feof( in_file ) ) {
            free( out_font );
            out_font = NULL;
            return( out_font );
        }

        /* The file is now positioned at the start of the data */

        switch ( width_data_size ) {
        case 0x81:
            /* The WidthBlock has one-byte elements */

            /* The last byte read should be 0x00 */
            
            if( count8 != 0x00 ) {
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Get the width data array into an array of bytes */

            fread( &uint8_array, sizeof( uint8_array ), 1, in_file );
            if( ferror( in_file ) || feof( in_file ) ) {
                free( out_font );
               out_font = NULL;
               return( out_font );
            }

            /* Copy the data into the cop_font instance */
            for( i = 0; i < 0x100; i++ ) {
                width_ptr[i] = uint8_array[i];
            }

            break;
        case 0x82:
            /* The WidthBlock has two-byte elements */

            /* The last byte read should be 0x00 */
            
            if( count8 != 0x00 ) {
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Get the width data array into an array of two-byte entries */

            fread( &uint16_array, sizeof( uint16_array ), 1, in_file );
            if( ferror( in_file ) || feof( in_file ) ) {
                free( out_font );
               out_font = NULL;
               return( out_font );
            }

            /* Copy the data into the cop_font instance */
            for( i = 0; i < 0x100; i++ ) {
                width_ptr[i] = uint16_array[i];
            }

            break;
        case 0x84:
            /* The WidthBlock has two-byte elements */

            /* The last byte read should be 0x01 */
            
            if( count8 != 0x01 ) {
                free( out_font );
                out_font = NULL;
                return( out_font );
            }

            /* Get the width data array directly into the cop_font instance */

            fread( width_ptr, sizeof( out_font->width->table ), 1, in_file );
            if( ferror( in_file ) || feof( in_file ) ) {
                free( out_font );
               out_font = NULL;
               return( out_font );
            }
            break;
        default:
            printf_s( "Bad size indicator for Width Block: %i\n", count8 );
            free( out_font );
            out_font = NULL;
            return( out_font );
        }
    }

    /* Convert non-NULL offsets to pointers */

    if( out_font->font_out_name1 != NULL ) {
        string_ptr = (char *) out_font + (size_t) out_font->font_out_name1;
        out_font->font_out_name1 = string_ptr;
    }

    if( out_font->font_out_name2 != NULL ) {
        string_ptr = (char *) out_font + (size_t) out_font->font_out_name2;
        out_font->font_out_name2 = string_ptr;
    }

    if( out_font->intrans != NULL ) {
        byte_ptr = (uint8_t *) out_font + (size_t) out_font->intrans;
        out_font->intrans = (intrans_block *) byte_ptr;
    }
    
    if( out_font->outtrans != NULL ) {
        byte_ptr = (uint8_t *) out_font + (size_t) out_font->outtrans;
        out_font->outtrans = (outtrans_block *) byte_ptr;
    
        for( i = 0; i < sizeof( outtrans_block ) / sizeof( translation * ); i++ ) {
            if( out_font->outtrans->table[i] != NULL ) {
                byte_ptr = (uint8_t *) out_font + (size_t) out_font->outtrans->table[i];
                out_font->outtrans->table[i] = (translation *) byte_ptr;
                if( out_font->outtrans->table[i]->data != NULL ) {
                    byte_ptr = (uint8_t *) out_font + (size_t) out_font->outtrans->table[i]->data;
                    out_font->outtrans->table[i]->data = byte_ptr;
                }
            }
        }
    }

    if( out_font->width != NULL ) {
        byte_ptr = (uint8_t *) out_font + (size_t) out_font->width;
        out_font->width = (width_block *) byte_ptr;
    }
    
    
    return( out_font );
}

/*  Function resize_cop_font().
 *  Resizes a cop_font instance.
 *
 *  Parameters:
 *      in_font is a pointer to the cop_font to be resized
 *      in_size is the minimum acceptable increase in size
 *
 *  Warning:
 *      If realloc() returns a different value from in_font, then the
 *      memory pointed to by in_font will be freed whether the function
 *      succeeds or fails. The intended use is for the pointer passed as
 *      in_font to be used to store the return value. 
 *
 *  Returns:
 *      A pointer to a cop_font instance at least in_size larger with the same
 *          data (except for the allocated_size field, which reflects the new size)
 *          on success.
 *      A NULL pointer on failure.
 */

cop_font * resize_cop_font( cop_font * in_font, size_t in_size )
{
    cop_font *  local_font  = NULL;
    size_t      increment   = INC_SIZE;
    size_t      new_size;
    size_t      scale;

    /* Compute how much larger to make the cop_font struct */

    if( in_size > INC_SIZE ) {
        scale = in_size / INC_SIZE;
        ++scale;
        increment = scale * INC_SIZE;
    }
    new_size = in_font->allocated_size + increment;

    /* Reallocate the cop_font */

    local_font = (cop_font *) realloc( in_font, new_size );
    if( local_font != in_font ) free( in_font );
    if( local_font != NULL ) local_font->allocated_size = new_size;

    return( local_font );
}
