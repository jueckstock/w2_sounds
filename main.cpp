#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <stdexcept>

#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>

// Stupid macros!
static bool al_init_wrapper() {
	return al_init();
}

// Table of startup routines/messages
static const struct startup_t {
	bool (*proc)();
	const char *msg;
} startups[] = {
	{ al_init_wrapper, "Initializing Allegro system..." },
	{ al_install_audio, "Initializing audio subsystem..." },
	{ nullptr, nullptr }
};

void startup() {
	for (const startup_t *s = startups; s->proc != nullptr; ++s) {
		std::cout << s->msg;
		if (s->proc()) {
			std::cout << "OK\n";
		}
		else {
			std::cout << "FAILED (errno=" << al_get_errno() << ")\n";
			al_uninstall_system();
			exit(1);
		}
	}
}

static constexpr int FREQUENCY{ 11025 };

// Locations/sizes of 8-bit PCM (11.025 KHz) audio samples in Wetspot2's RESOURCE.BIN file
static const struct sample_loc_t {
	size_t offset;
	size_t length;
} samples[] = {
	{ 4802, 1470 },
	{ 6272, 1714 },
	{ 7986, 6386 },
	{ 14372, 9456 },
	{ 23828, 9488 },
	{ 33316, 7824 },
	{ 41140, 3674 },
	{ 44814, 12338 },
	{ 57152, 3256 },
	{ 60408, 28864 },
	{ 89272, 26816 },
	{ 116088, 18048 },
	{ 134136, 3690 },
	{ 137826, 15822 },
	{ 153648, 4694 },
	{ 158342, 1754 },
	{ 160096, 10020 },
	{ 170116, 5782 },
	{ 175898, 9584 },
};

static constexpr size_t NUM_SOUNDS = sizeof(samples) / sizeof(samples[0]);

// Template specialization: have ALLEGRO_SAMPLE ptrs deleted with "al_destroy_sample(...)"
namespace std {
	template<>
	class default_delete<ALLEGRO_SAMPLE>
	{
	public:
		void operator()(ALLEGRO_SAMPLE *sptr) {
			al_destroy_sample(sptr);
		}
	};
}

// Use a vector of char for all file I/O (because stupid iostreams API)
using Buffer = std::vector<char>;

// Use unique_ptr to manage the lifetime of ALLEGRO_SAMPLE objects
using SamplePtr = std::unique_ptr<ALLEGRO_SAMPLE>;

// Utility function to open and read the entire [binary] contents
// of a given file into a vector<char> (resizing as necessary)
bool slurp_file(const char *filename, Buffer& dest) {
	std::ifstream file{ filename, std::ios::binary };
	if (!file.seekg(0, std::ios::end)) { return false; }
	auto file_size = file.tellg();
	dest.resize(file_size);
	if (!file.seekg(0, std::ios::beg)) { return false; }
	file.read(&dest[0], dest.size());
	return file.good();	// Have not read PAST EOF, so should still be good...
}

// Abstraction of all resources packed into RESOURCE.BIN
class ResourceBin {
	// Raw backing store (entire file read into memory)
	Buffer data_;

	// Audio samples
	std::vector<SamplePtr> wavs_;
public:
	explicit ResourceBin(const char *pathToResourceBin = "RESOURCE.BIN") {
		if (!slurp_file(pathToResourceBin, data_)) {
			throw std::exception("Unable to load RESOURCE.BIN data");
		}

		// Create ALLEGRO_SAMPLE objects for each raw sample contained in our data
		for (size_t i = 0; i < NUM_SOUNDS; ++i) {
			void *sample_start = &data_[samples[i].offset];
			wavs_.emplace_back(al_create_sample(sample_start, samples[i].length, FREQUENCY, ALLEGRO_AUDIO_DEPTH_UINT8, ALLEGRO_CHANNEL_CONF_1, false));
		}
	}

	ALLEGRO_SAMPLE *sample(size_t index) const {
		return wavs_.at(index).get();
	}

	size_t num_samples() const {
		return NUM_SOUNDS;
	}

};

int main(int argc, char **argv) {
	startup();

	ResourceBin rsrc{ "RESOURCE.BIN" };

	if (!al_reserve_samples(rsrc.num_samples())) {
		std::cout << "Failed to reserve " << rsrc.num_samples() << " samples (errno=" << al_get_errno() << ")\n";
		exit(1);
	}

	std::cout << "Enter a sample index [0, " << rsrc.num_samples() << "); invalid to quit: ";
	int sound_index = 0;
	while (std::cin >> sound_index) {
		if ((sound_index < 0) || (sound_index >= rsrc.num_samples())) { break; }
		al_play_sample(rsrc.sample(sound_index), 1.0f, ALLEGRO_AUDIO_PAN_NONE, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
		std::cout << "Enter a sample index [0, " << rsrc.num_samples() << "): ";
	}

	return 0;
}
