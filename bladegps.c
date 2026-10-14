#define _CRT_SECURE_NO_WARNINGS

#include <signal.h>
#include <setjmp.h>
#include <sched.h>
#include <assert.h>
#include "bladegps.h"

// for _getch used in Windows runtime.
#ifdef _WIN32
#include <conio.h>
#include "getopt.h"
#else
#include <unistd.h>
#endif

uint32_t tx_samplerate = (uint32_t)2.6e6;
uint32_t num_iq_samples;
uint32_t fifo_length;
volatile sig_atomic_t eflag = 0;

void init_sim(sim_t *s)
{
	s->tx.dev = NULL;
	pthread_mutex_init(&(s->tx.lock), NULL);
	//s->tx.error = 0;

	pthread_mutex_init(&(s->gps.lock), NULL);
	//s->gps.error = 0;
	s->gps.ready = 0;
	pthread_cond_init(&(s->gps.initialization_done), NULL);

	s->status = 0;
	s->head = 0;
	s->tail = 0;
	s->sample_length = 0;

	pthread_cond_init(&(s->fifo_write_ready), NULL);
	pthread_cond_init(&(s->fifo_read_ready), NULL);

	s->time = 0.0;
}

size_t get_sample_length(sim_t *s)
{
	long length;

	length = s->head - s->tail;
	if (length < 0)
		length += fifo_length;

	return((size_t)length);
}

size_t fifo_read(int16_t *buffer, size_t samples, sim_t *s)
{
	size_t length;
	size_t samples_remaining;
	int16_t *buffer_current = buffer;

	length = get_sample_length(s);

	if (length < samples)
		samples = length;

	length = samples; // return value

	samples_remaining = fifo_length - s->tail;

	if (samples > samples_remaining) {
		memcpy(buffer_current, &(s->fifo[s->tail * 2]), samples_remaining * sizeof(int16_t) * 2);
		s->tail = 0;
		buffer_current += samples_remaining * 2;
		samples -= samples_remaining;
	}

	memcpy(buffer_current, &(s->fifo[s->tail * 2]), samples * sizeof(int16_t) * 2);
	s->tail += (long)samples;
	if (s->tail >= fifo_length)
		s->tail -= fifo_length;

	return(length);
}

bool is_finished_generation(sim_t *s)
{
	return s->finished;
}

int is_fifo_write_ready(sim_t *s)
{
	int status = 0;

	s->sample_length = get_sample_length(s);
	if (s->sample_length < num_iq_samples)
		status = 1;

	return(status);
}

void *tx_task(void *arg)
{
	sim_t *s = (sim_t *)arg;
	size_t samples_populated;

	while (1) {
		int16_t *tx_buffer_current = s->tx.buffer;
		unsigned int buffer_samples_remaining = SAMPLES_PER_BUFFER;

		while (buffer_samples_remaining > 0) {

			pthread_mutex_lock(&(s->gps.lock));
			while (get_sample_length(s) == 0)
			{
				pthread_cond_wait(&(s->fifo_read_ready), &(s->gps.lock));
			}

			samples_populated = fifo_read(tx_buffer_current,
				buffer_samples_remaining,
				s);
			pthread_mutex_unlock(&(s->gps.lock));

			pthread_cond_signal(&(s->fifo_write_ready));
#if 0
			if (is_fifo_write_ready(s)) {
				/*
				printf("\rTime = %4.1f", s->time);
				s->time += 0.1;
				fflush(stdout);
				*/
			}
			else if (is_finished_generation(s))
			{
				goto out;
			}
#endif
			// Advance the buffer pointer.
			buffer_samples_remaining -= (unsigned int)samples_populated;
			tx_buffer_current += (2 * samples_populated);
		}
		assert(buffer_samples_remaining == 0);

		// If there were no errors, transmit the data buffer.
		bladerf_sync_tx(s->tx.dev, s->tx.buffer, SAMPLES_PER_BUFFER, NULL, TIMEOUT_MS);
		// TODO: what is this if sentence for?
		if (is_fifo_write_ready(s)) {
			/*
			printf("\rTime = %4.1f", s->time);
			s->time += 0.1;
			fflush(stdout);
			*/
		}
		else if (is_finished_generation(s))
		{
			goto out;
		}

	}
out:
	return NULL;
}

int start_tx_task(sim_t *s)
{
	int status;

	status = pthread_create(&(s->tx.thread), NULL, tx_task, s);

	return(status);
}

int start_gps_task(sim_t *s)
{
    int status;
    // struct sched_param param;
    // int policy = SCHED_FIFO;  // Real-time FIFO policy

    status = pthread_create(&(s->gps.thread), NULL, gps_task, s);

    // Set scheduling priority
    // param.sched_priority = sched_get_priority_max(policy);
    // if (pthread_setschedparam(s->gps.thread, policy, &param) != 0)
    //     {
    //         perror("Failed to set thread scheduling");
    //         exit(EXIT_FAILURE);
    //     }

    // pthread_join(s->gps.thread, NULL);

    return(status);
}

void usage(void)
{
	printf("Usage: bladegps [options]\n"
		"Options:\n"
		"  -e <gps_nav>     RINEX navigation file for GPS ephemerides (required)\n"
		"  -y <yuma_alm>    YUMA almanac file for GPS almanacs\n"
		"  -u <user_motion> User motion file (dynamic mode) for Ch1\n"
		"  -U <user_motion> User motion file (dynamic mode) for Ch2\n"
		"  -s <log_dir>     Log directory of user position (dynamic mode)\n"
		"  -g <nmea_gga>    NMEA GGA stream (dynamic mode)\n"
		"  -l <location>    Lat,Lon,Hgt (static mode) e.g. 35.274,137.014,100\n"
		"  -L <location>    Lat,Lon,Hgt (static mode) of Ch2 e.g. 35.274,137.014,100\n"
		"  -t <date,time>   Scenario start time YYYY/MM/DD,hh:mm:ss\n"
		"  -T <date,time>   Overwrite TOC and TOE to scenario start time\n"
		"  -d <duration>    Duration [sec] (max: %.0f)\n"
		"  -x <XB number>   Enable expansion board, e.g. '-x 200' for XB200\n"
		"  -b <devstr>      bladeRF device string (standalone use, default: first device)\n"
		"  -D <data_dir>    Directory of gps_table.csv and antenna/ (default: $BLADEGPS_DATA_DIR or ./gnss)\n"
		"  -a <tx_gain>     TX Gain (default: %d)\n"
		"  -r <azi,ele>     Rx antenna attitude in degree (default: azi=0, ele=90)\n"
		"  -R <azi,ele>     Rx antenna attitude in degree of Ch2 (default: same as Ch1)\n"
		"  -i               Interactive mode: North='%c', South='%c', East='%c', West='%c'\n"
		"  -I               Disable ionospheric delay for spacecraft scenario\n"
		"  -E               Equal distribution of rx and tx antenna pattern\n"
		"  -p               Disable path loss and hold power level constant\n"
		"  -j               User motion files are in ECI: angle[rad],x,y,z,vx,vy,vz (z-rotation by angle from ECEF)\n"
		"  -f               Fast option, run only simulation part as fast as possible without generating IQ (no device needed).\n"
		"  -v               Verbosity option\n",
		((double)USER_MOTION_SIZE)/10.0,
		TX_GAIN,
		NORTH_KEY, SOUTH_KEY, EAST_KEY, WEST_KEY);

	return;
}

#ifndef GIT_COMMIT_ID
#define GIT_COMMIT_ID "unknown"
#endif
#ifndef GIT_COMMIT_DATE
#define GIT_COMMIT_DATE ""
#endif

static void bladegps_opening()
{
	printf("\nbladeGPS is now starting. commit=%s (%s)\n", GIT_COMMIT_ID, GIT_COMMIT_DATE);
}

// sim_t *simp = NULL;
// void bladeGPS_signal_handler(int sig)
// {
//     const bladerf_gain tx_mingain = -22;
// 	// bladerf_set_gain(simp->tx.dev, BLADERF_CHANNEL_TX(0), tx_mingain);
// 	bladerf_set_gain(simp->tx.dev, BLADERF_CHANNEL_TX(0), tx_mingain);
// 	// printf("[AOWRTX] Set TX1 gain at the minimum level.\n");
// 	eflag = 1;
// }

int bladegps_main(struct bladerf *dev, int argc, char *argv[])
{
	sim_t s = {0};
	char *devstr = NULL;
	const bool own_dev = (dev == NULL); // Open/close the device here unless the caller owns it.
	int xb_board = 0;

	int result;
	double duration;
	datetime_t t0;

	const struct bladerf_range *range = NULL;
	double min_gain;
	double max_gain;
	int tx_gain = TX_GAIN;
	bladerf_channel tx_channel = BLADERF_CHANNEL_TX(0);

	if (argc<3)
	{
		usage();
		exit(1);
	}
	bladegps_opening();

	// if (signal(SIGINT, bladeGPS_signal_handler) == SIG_ERR &&
	// 	signal(SIGTERM, bladeGPS_signal_handler) == SIG_ERR) {
	// 	fprintf(stderr, "Failed to set up signal handler\n");
	// 	exit(1);
	// }
	// simp = &s;
	s.finished = false;

	s.opt.navfile[0] = 0;
	s.opt.almfile[0] = 0;
	s.opt.umfile[0] = 0;
	s.opt.g0.week = -1;
	s.opt.g0.sec = 0.0;
	s.opt.iduration = USER_MOTION_SIZE;
	s.opt.verb = FALSE;
	s.opt.nmeaGGA = FALSE;
	s.opt.staticLocationMode = TRUE; // default user motion
	s.opt.llh[0] = 40.7850916 * D2R;
	s.opt.llh[1] = -73.968285 * D2R;
	s.opt.llh[2] = 100.0;
	s.opt.interactive = FALSE;
	s.opt.timeoverwrite = FALSE;
	s.opt.iono_enable = TRUE;
	s.opt.antenna_pattern_enable = true;
	s.opt.path_loss_enable = TRUE;
	s.opt.rec_ant_dir[0] = 0.0;
	s.opt.rec_ant_dir[0] = 90.0 *D2R;
	option_t opt2 = s.opt;
	s.ch2_enable = false;
	s.fast_mode = false;

	while ((result=getopt(argc,argv,":e:y:u:U:s:g:l:L:T:t:d:x:a:r:R:b:D:iIEpfjv"))!=-1)
	{
		switch (result)
		{
		case 'e':
			strcpy(s.opt.navfile, optarg);
			break;
		case 'y':
			strcpy(s.opt.almfile, optarg);
			break;
		case 'u':
			strcpy(s.opt.umfile, optarg);
			s.opt.nmeaGGA = FALSE;
			s.opt.staticLocationMode = FALSE;
			break;
		case 'U':
			s.ch2_enable = true;
			strcpy(opt2.umfile, optarg);
			opt2.nmeaGGA = FALSE;
			opt2.staticLocationMode = FALSE;
			break;
		case 's':
			strcpy(s.opt.log_dir, optarg);
			s.opt.nmeaGGA = FALSE;
			// FIXME: how about opt2?
			break;
		case 'g':
			strcpy(s.opt.umfile, optarg);
			s.opt.nmeaGGA = TRUE;
			s.opt.staticLocationMode = FALSE;
			break;
		case 'l':
			// Static geodetic coordinates input mode
			// Added by scateu@gmail.com
			s.opt.nmeaGGA = FALSE;
			s.opt.staticLocationMode = TRUE;
			sscanf(optarg,"%lf,%lf,%lf",&s.opt.llh[0],&s.opt.llh[1],&s.opt.llh[2]);
			s.opt.llh[0] *= D2R; // convert to RAD
			s.opt.llh[1] *= D2R; // convert to RAD
			break;
		case 'L':
			// Static geodetic coordinates input mode in ch2
			s.ch2_enable = true;
			opt2.nmeaGGA = FALSE;
			opt2.staticLocationMode = TRUE;
			sscanf(optarg,"%lf,%lf,%lf",&opt2.llh[0],&opt2.llh[1],&opt2.llh[2]);
			opt2.llh[0] *= D2R; // convert to RAD
			opt2.llh[1] *= D2R; // convert to RAD
			break;
		case 'T':
			s.opt.timeoverwrite = TRUE;
			if (strncmp(optarg, "now", 3)==0)
			{
				time_t timer;
				struct tm *gmt;

				time(&timer);
				gmt = gmtime(&timer);

				t0.y = gmt->tm_year+1900;
				t0.m = gmt->tm_mon+1;
				t0.d = gmt->tm_mday;
				t0.hh = gmt->tm_hour;
				t0.mm = gmt->tm_min;
				t0.sec = (double)gmt->tm_sec;

				date2gps(&t0, &s.opt.g0);

				break;
			}
		case 't':
			sscanf(optarg, "%d/%d/%d,%d:%d:%lf", &t0.y, &t0.m, &t0.d, &t0.hh, &t0.mm, &t0.sec);
			if (t0.y<=1980 || t0.m<1 || t0.m>12 || t0.d<1 || t0.d>31 ||
				t0.hh<0 || t0.hh>23 || t0.mm<0 || t0.mm>59 || t0.sec<0.0 || t0.sec>=60.0)
			{
				printf("ERROR: Invalid date and time.\n");
				exit(1);
			}
			t0.sec = floor(t0.sec);
			date2gps(&t0, &s.opt.g0);
			break;
		case 'd':
			duration = atof(optarg);
			if (duration<0.0 || duration>((double)USER_MOTION_SIZE)/10.0)
			{
				printf("ERROR: Invalid duration.\n");
				exit(1);
			}
			s.opt.iduration = (int)(duration*10.0+0.5);
			break;
		case 'x':
			xb_board=atoi(optarg);
			break;
		case 'b':
			devstr = optarg;
			break;
		case 'D':
			SetGnssDataDir(optarg);
			break;
		case 'a':
			tx_gain = atoi(optarg);
			// NOTE: probably not to break bladerf?
			// if (tx_gain>0)
			// 	tx_gain *= -1;
			break;
		case 'r':
			sscanf(optarg,"%lf,%lf",&s.opt.rec_ant_dir[0],&s.opt.rec_ant_dir[1]);
			s.opt.rec_ant_dir[0] *= D2R; // convert to RAD
			s.opt.rec_ant_dir[1] *= D2R; // convert to RAD
			break;
		case 'R':
			s.ch2_enable = true;
			sscanf(optarg,"%lf,%lf",&opt2.rec_ant_dir[0],&opt2.rec_ant_dir[1]);
			opt2.rec_ant_dir[0] *= D2R; // convert to RAD
			opt2.rec_ant_dir[1] *= D2R; // convert to RAD
			break;
		case 'i':
			s.opt.interactive = TRUE;
			break;
		case 'I':
			s.opt.iono_enable = FALSE; // Disable ionospheric correction
			opt2.iono_enable = FALSE;
			break;
		case 'E':
			s.opt.antenna_pattern_enable = false;
			opt2.antenna_pattern_enable = false;
			break;
		case 'p':
			s.opt.path_loss_enable = FALSE; // Disable path loss
			opt2.path_loss_enable = FALSE;
			break;
		case 'f':
			s.fast_mode = true;
			break;
		case 'j':
			s.opt.eci_input = true;
			opt2.eci_input = true;
			break;
		case 'v':
			s.opt.verb = TRUE;
			break;
		case ':':
			printf("option needs a value\n");
			usage();
			exit(1);
		case '?':
			printf("unknown option: %c\n", optopt);
			usage();
			exit(1);
		default:
			break;
		}
	}

	if (s.opt.navfile[0]==0)
	{
		printf("ERROR: GPS ephemeris file is not specified.\n");
		exit(1);
	}

	if ((s.opt.umfile[0]==0 && !s.opt.staticLocationMode) ||
		(opt2.umfile[0]==0 && !opt2.staticLocationMode))
	{
		printf("ERROR: User motion file / NMEA GGA stream is not specified.\n");
		printf("You may use -l to specify the static location directly.\n");
		exit(1);
	}

	if (s.ch2_enable)
	{
		s.opt2 = opt2;
	}

	// Initialize simulator
	init_sim(&s);
	s.tx.dev = dev;
	s.status = 0;

	if (s.fast_mode)
	{
		// Simulation only. No IQ samples are generated, so the device is not used.
		if (dev != NULL)
			bladerf_get_sample_rate(dev, tx_channel, &tx_samplerate);
		num_iq_samples = (tx_samplerate / 10);
		fifo_length = s.ch2_enable ? (num_iq_samples * 2 * 2) : (num_iq_samples * 2);

		s.status = start_gps_task(&s);
		if (s.status < 0) {
			fprintf(stderr, "Failed to start GPS task.\n");
			return(1);
		}
		pthread_join(s.gps.thread, NULL);
		printf("\nDone!\n");
		return(s.gps.ready < 0 ? 1 : 0);
	}

	if (own_dev)
	{
		printf("Opening device...\n");
		s.status = bladerf_open(&s.tx.dev, devstr);
		if (s.status != 0) {
			fprintf(stderr, "Failed to open device: %s\n", bladerf_strerror(s.status));
			s.tx.dev = NULL;
			goto out;
		}

		s.status = bladerf_set_sample_rate(s.tx.dev, tx_channel, TX_SAMPLERATE, NULL);
		if (s.status != 0) {
			fprintf(stderr, "Failed to set TX sample rate: %s\n", bladerf_strerror(s.status));
			goto out;
		}
	}

	// NOTE: only checking of ch1 because both are the same.
	// Get sample rate for buffer size. When the device is given by the caller,
	// the sample rate set by the caller (e.g. the same as the receiver) is used.
	s.status = bladerf_get_sample_rate(s.tx.dev, tx_channel, &tx_samplerate);
	if (s.status != 0) {
		fprintf(stderr, "Failed to get TX sample rate: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX sample rate: %u sps\n", tx_samplerate);
	}

	num_iq_samples = (tx_samplerate / 10);
    // NOTE: fifo_length should be double of buffer size.
	fifo_length = s.ch2_enable ? (num_iq_samples * 2 * 2) : (num_iq_samples * 2);

	// Allocate FIFOs to hold 0.1 seconds of I/Q samples each.
	s.fifo = (int16_t *)malloc(fifo_length * sizeof(int16_t) * 2); // for 16-bit I and Q samples

	if (s.fifo == NULL) {
		fprintf(stderr, "Failed to allocate I/Q sample buffer.\n");
		goto out;
	}


	// Allocate TX buffer to hold each block of samples to transmit.
	s.tx.buffer = (int16_t *)malloc(SAMPLES_PER_BUFFER * sizeof(int16_t) * 2); // for 16-bit I and Q samples

	if (s.tx.buffer == NULL) {
		fprintf(stderr, "Failed to allocate TX buffer.\n");
		goto out;
	}

	// Initializing device.
	printf("Initializing device...\n");

    // FIXME: not used currently, so not set for the ch2 even if the ch2 is used.
	if(xb_board == 200) {
		printf("Initializing XB200 expansion board...\n");

		s.status = bladerf_expansion_attach(s.tx.dev, BLADERF_XB_200);
		if (s.status != 0) {
			fprintf(stderr, "Failed to enable XB200: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		s.status = bladerf_xb200_set_filterbank(s.tx.dev, tx_channel, BLADERF_XB200_CUSTOM);
		if (s.status != 0) {
			fprintf(stderr, "Failed to set XB200 TX filterbank: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		s.status = bladerf_xb200_set_path(s.tx.dev, tx_channel, BLADERF_XB200_BYPASS);
		if (s.status != 0) {
			fprintf(stderr, "Failed to enable TX bypass path on XB200: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		//For sake of completeness set also RX path to a known good state.
		s.status = bladerf_xb200_set_filterbank(s.tx.dev, BLADERF_CHANNEL_RX(0), BLADERF_XB200_CUSTOM);
		if (s.status != 0) {
			fprintf(stderr, "Failed to set XB200 RX filterbank: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		s.status = bladerf_xb200_set_path(s.tx.dev, BLADERF_CHANNEL_RX(0), BLADERF_XB200_BYPASS);
		if (s.status != 0) {
			fprintf(stderr, "Failed to enable RX bypass path on XB200: %s\n", bladerf_strerror(s.status));
			goto out;
		}
	}

	if(xb_board == 300) {
		fprintf(stderr, "XB300 does not support transmitting on GPS frequency\n");
		goto out;
	}

	// NOTE: set frequency only on tx ch1 even though using both channels,
	// because they become the same once either of them is set.
	s.status = bladerf_set_frequency(s.tx.dev, tx_channel, TX_FREQUENCY);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set TX frequency: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX frequency: %u Hz\n", TX_FREQUENCY);
	}

	// NOTE: set bandwidth only on tx ch1 even though using both channels,
	// because of the same reason as frequency.
	s.status = bladerf_set_bandwidth(s.tx.dev, tx_channel, TX_BANDWIDTH, NULL);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set TX bandwidth: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX bandwidth: %u Hz\n", TX_BANDWIDTH);
	}

	// NOTE: gain range is mutual, so only required for ch1.
	s.status = bladerf_get_gain_range(s.tx.dev, tx_channel, &range);
	if (s.status != 0) {
	fprintf(stderr, "Failed to check gain range: %s\n", bladerf_strerror(s.status));
	goto out;
	}
	else {
		min_gain = range->min * range->scale;
		max_gain = range->max * range->scale;
		printf("TX gain range: [%g dB, %g dB] \n",min_gain, max_gain);
		if (tx_gain < min_gain)
			tx_gain = min_gain;
		else if (tx_gain > max_gain)
			tx_gain = max_gain;
	}

	s.status = bladerf_set_gain(s.tx.dev, tx_channel, tx_gain);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set gain: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		int tx1_gain;
		s.status = bladerf_get_gain(s.tx.dev, tx_channel, &tx1_gain);
		printf("TX1 gain: %d dB\n", tx1_gain);
	}

	// NOTE: use the same tx gain as ch1, attenuation according to the distance, etc. should be considered in gpssim.
    int tx2_gain = s.ch2_enable ? tx_gain : -22;

	s.status = bladerf_set_gain(s.tx.dev, BLADERF_CHANNEL_TX(1), tx2_gain);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set gain: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		s.status = bladerf_get_gain(s.tx.dev, BLADERF_CHANNEL_TX(1), &tx2_gain);
		printf("TX2 gain: %d dB\n", tx2_gain);
	}

	// Start GPS task.
	s.status = start_gps_task(&s);
	if (s.status < 0) {
		fprintf(stderr, "Failed to start GPS task.\n");
		goto out;
	}
	else
		printf("Creating GPS task...\n");

	// Wait until GPS task is initialized
	pthread_mutex_lock(&(s.tx.lock));
	while (!s.gps.ready)
		pthread_cond_wait(&(s.gps.initialization_done), &(s.tx.lock));
	pthread_mutex_unlock(&(s.tx.lock));

	if (s.gps.ready < 0) {
		fprintf(stderr, "GPS task failed to initialize.\n");
		s.status = -1;
		goto out;
	}

	// Fulfill the FIFO.
	if (is_fifo_write_ready(&s))
		pthread_cond_signal(&(s.fifo_write_ready));

	// Configure the TX module for use with the synchronous interface.
	bladerf_channel_layout tx_ch_layout = s.ch2_enable ? BLADERF_TX_X2 : BLADERF_TX_X1;
	s.status = bladerf_sync_config(s.tx.dev,
			tx_ch_layout,
			BLADERF_FORMAT_SC16_Q11,
			NUM_BUFFERS,
			SAMPLES_PER_BUFFER,
			NUM_TRANSFERS,
			TIMEOUT_MS);

	if (s.status != 0) {
		fprintf(stderr, "Failed to configure TX sync interface: %s\n", bladerf_strerror(s.status));
		goto out;
	}

	// We must always enable the modules *after* calling bladerf_sync_config().
	s.status = bladerf_enable_module(s.tx.dev, tx_channel, true);
	if (s.status != 0) {
		fprintf(stderr, "Failed to enable TX module: %s\n", bladerf_strerror(s.status));
		goto out;
	}
    if (s.ch2_enable)
    {
        s.status = bladerf_enable_module(s.tx.dev, BLADERF_CHANNEL_TX(1), true);
        if (s.status != 0) {
            fprintf(stderr, "Failed to enable TX module at ch2: %s\n", bladerf_strerror(s.status));
            goto out;
        }
    }

	// Start TX task
	s.status = start_tx_task(&s);
	if (s.status < 0) {
		fprintf(stderr, "Failed to start TX task.\n");
		goto out;
	}
	else
		printf("Creating TX task...\n");

	// Running...
	printf("Running...\n");
	printf("Press 'q' to quit.\n");

	// Waiting for TX task to complete.
	pthread_join(s.tx.thread, NULL);
	printf("\nDone!\n");

	// Disable TX module and shut down underlying TX stream.
	s.status = bladerf_enable_module(s.tx.dev, tx_channel, false);
	if (s.status != 0)
		fprintf(stderr, "Failed to disable TX module: %s\n", bladerf_strerror(s.status));
    if (s.ch2_enable)
    {
        s.status = bladerf_enable_module(s.tx.dev, BLADERF_CHANNEL_TX(1), false);
        if (s.status != 0) {
            fprintf(stderr, "Failed to disable TX module at ch2: %s\n", bladerf_strerror(s.status));
            goto out;
        }
    }

out:
	// Free up resources
	if (s.tx.buffer != NULL)
		free(s.tx.buffer);

	if (s.fifo != NULL)
		free(s.fifo);

	if (s.tx.dev != NULL) {
		bladerf_set_gain(s.tx.dev, BLADERF_CHANNEL_TX(0), -22);
		printf("Set TX1 gain at minimum level.\n");

		if (own_dev) {
			printf("Closing device...\n");
			bladerf_close(s.tx.dev);
		}
	}

	return(s.status == 0 ? 0 : 1);
}
