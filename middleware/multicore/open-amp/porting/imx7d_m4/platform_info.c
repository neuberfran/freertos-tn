/*
 * Copyright (c) 2015 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of Freescale Semiconductor nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**************************************************************************
 * FILE NAME
 *
 *       platform_info.c
 *
 * DESCRIPTION
 *
 *       This file implements APIs to get platform specific
 *       information for OpenAMP.
 *
 **************************************************************************/

#include "platform.h"
#include "rpmsg/rpmsg.h"

/* Reference implementation that show cases platform_get_cpu_info and
 platform_get_for_firmware API implementation for Bare metal environment */

extern struct hil_platform_ops proc_ops;

/*
 * Linux requires the ALIGN to 0x1000(4KB) instead of 0x80
 */
#define VRING_ALIGN                       0x1000

/*
 * Linux has a different alignment requirement, and its have 512 buffers instead of 32 buffers for the 2 ring
 */
#define VRING0_BASE                       0x9FFF0000
#define VRING1_BASE                       0x9FFF8000

/* IPI_VECT here defines VRING index in MU */
#define VRING0_IPI_VECT                   0
#define VRING1_IPI_VECT                   1

#define MASTER_CPU_ID                     0
#define REMOTE_CPU_ID                     1

/**
 * This array provides definition of CPU nodes for master and remote
 * context. It contains two nodes because the same file is intended
 * to use with both master and remote configurations. On zynq platform
 * only one node definition is required for master/remote as there
 * are only two cores present in the platform.
 *
 * Only platform specific info is populated here. Rest of information
 * is obtained during resource table parsing.The platform specific
 * information includes;
 *
 * -CPU ID
 * -Shared Memory
 * -Interrupts
 * -Channel info.
 *
 * Although the channel info is not platform specific information
 * but it is convenient to keep it in HIL so that user can easily
 * provide it without modifying the generic part.
 *
 * It is good idea to define hil_proc structure with platform
 * specific fields populated as this can be easily copied to hil_proc
 * structure passed as parameter in platform_get_processor_info. The
 * other option is to populate the required structures individually
 * and copy them one by one to hil_proc structure in platform_get_processor_info
 * function. The first option is adopted here.
 *
 *
 * 1) First node in the array is intended for the remote contexts and it
 *    defines Master CPU ID, shared memory, interrupts info, number of channels
 *    and there names. This node defines only one channel
 *   "rpmsg-data-channel".
 *
 * 2)Second node is required by the master and it defines remote CPU ID,
 *   shared memory and interrupts info. In general no channel info is required by the
 *   Master node, however in bare-metal master and linux remote case the linux
 *   rpmsg bus driver behaves as master so the rpmsg driver on linux side still needs
 *   channel info. This information is not required by the masters for bare-metal
 *   remotes.
 *
 */
struct hil_proc proc_table []=
{
    /* CPU node for remote context */
    {
        /* CPU ID of master */
        MASTER_CPU_ID,

        /* Shared memory info - Last field is not used currently */
        {
            (void*)SHM_ADDR, SHM_SIZE, 0x00
        },

        /* VirtIO device info */        /*struct proc_vdev*/
        {
            2, (1<<VIRTIO_RPMSG_F_NS), 0,        /*num_vring, dfeatures, gfeatures*/

            /* Vring info */                /*struct proc_vring*/
            {
                /*[0]*/
                { /* TX */
                    NULL, (void*)VRING0_BASE/*phy_addr*/, 256/*num_descs*/, VRING_ALIGN/*align*/,
                    /*struct virtqueue, phys_addr, num_descs, align*/
                    {
                        /*struct proc_intr*/
                        VRING0_IPI_VECT,0,0,NULL
                    }
                },
                /*[1]*/
                { /* RX */
                    NULL, (void*)VRING1_BASE, 256, VRING_ALIGN,
                    {
                        VRING1_IPI_VECT,0,0,NULL
                    }
                }
            }
        },

        /* Number of RPMSG channels */
        1,  /*num_chnls*/

        /* RPMSG channel info - Only channel name is expected currently */
        {
            {"rpmsg-openamp-demo-channel"}  /*chnl name*/
        },

        /* HIL platform ops table. */
        &proc_ops,  /*struct hil_platform_ops*/

        /* Next three fields are for future use only */
        0,
        0,
        NULL
    },

    /* CPU node for remote context */
    {
        /* CPU ID of remote */
        REMOTE_CPU_ID,

        /* Shared memory info - Last field is not used currently */
        {
            (void*)SHM_ADDR, SHM_SIZE, 0x00
        },

        /* VirtIO device info */
        {
            2, (1<<VIRTIO_RPMSG_F_NS), 0,
            {
                {/* RX */
                    NULL, (void*)VRING0_BASE, 256, VRING_ALIGN,
                    {
                        VRING0_IPI_VECT,0,0,NULL
                    }
                },
                {/* TX */
                    NULL, (void*)VRING1_BASE, 256, VRING_ALIGN,
                    {
                        VRING1_IPI_VECT,0,0,NULL
                    }
                }
            }
        },

        /* Number of RPMSG channels */
        1,

        /* RPMSG channel info - Only channel name is expected currently */
        {
            {"rpmsg-openamp-demo-channel"}
        },

        /* HIL platform ops table. */
        &proc_ops,

        /* Next three fields are for future use only */
        0,
        0,
        NULL
    }
};

/**
 * platform_get_processor_info
 *
 * Copies the target info from the user defined data structures to
 * HIL proc  data structure.In case of remote contexts this function
 * is called with the reserved CPU ID HIL_RSVD_CPU_ID, because for
 * remotes there is only one master.
 *
 * @param proc   - HIL proc to populate
 * @param cpu_id - CPU ID
 *
 * return  - status of execution
 */
int platform_get_processor_info(struct hil_proc *proc , int cpu_id)
{
    int idx;
    for(idx = 0; idx < sizeof(proc_table)/sizeof(struct hil_proc); idx++)
    {
        if((cpu_id == HIL_RSVD_CPU_ID) || (proc_table[idx].cpu_id == cpu_id))
        {
            env_memcpy(proc,&proc_table[idx], sizeof(struct hil_proc));
            return 0;
        }
    }
    return -1;
}

int platform_get_processor_for_fw(char *fw_name)
{
    return 1;
}
