#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "security.hpp"

using namespace yappi::security;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

signatures_t::signatures_t():
    m_context(EVP_MD_CTX_create())
{
    // Initialize error strings
    ERR_load_crypto_strings();

    // Load the credentials
    fs::path path = fs::path(config_t::get().paths.storage + ".tokens");
   
    if(!fs::exists(path)) {
        try {
            fs::create_directories(path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + path.string());
        }
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error(path.string() + " is not a directory");
    }

    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(path)), end;

    while(it != end) {
        fs::ifstream stream(it->path(), fs::ifstream::in);

        if(!stream) {
            syslog(LOG_ERR, "security: cannot open %s", it->path().string().c_str());
            ++it;
            continue;
        }

        EVP_PKEY* key = NULL;

#if BOOST_FILESYSTEM_VERSION == 3
        std::string identity = fs::basename(*it);
        std::string type = fs::extension(*it);
#else
        std::string filename = it->leaf();
        std::string identity = filename.substr(0, filename.find_last_of("."));
        std::string type = filename.substr(filename.find_last_of("."));
#endif
        std::ostringstream contents;
        
        contents << stream.rdbuf();
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(contents.str().data()), contents.str().length());
        
        if(type == ".public") {
            key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
            
            if(key == NULL) {
                syslog(LOG_ERR, "security: failed to load a public key from %s - %s",
                    it->path().string().c_str(), ERR_reason_error_string(ERR_get_error()));
            } else {    
                m_public_keys.insert(std::make_pair(identity, key));
            }
        } else if(type == ".private") {
            key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            
            if(key == NULL) {
                syslog(LOG_ERR, "security: failed to load a private key from %s - %s",
                    it->path().string().c_str(), ERR_reason_error_string(ERR_get_error()));
            } else {    
                m_private_keys.insert(std::make_pair(identity, key));
            }
        } else {
            syslog(LOG_WARNING, "security: unknown key type - '%s'", type.c_str());
        }

        BIO_free(bio);
        
        ++it;
    }
    
    syslog(LOG_NOTICE, "security: loaded %ld public key(s)", m_public_keys.size());
}

signatures_t::~signatures_t() {
    for(key_map_t::iterator it = m_public_keys.begin(); it != m_public_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }

    for(key_map_t::iterator it = m_private_keys.begin(); it != m_private_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }
    
    ERR_free_strings();
    EVP_MD_CTX_destroy(m_context);
}

std::string signatures_t::sign(const std::string& message, const std::string& token)
{
    key_map_t::const_iterator it = m_private_keys.find(token);

    if(it == m_private_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
 
    unsigned char buffer[EVP_PKEY_size(it->second)];
    unsigned int size = 0;
    
    EVP_SignInit(m_context, EVP_sha1());
    EVP_SignUpdate(m_context, message.data(), message.size());
    EVP_SignFinal(m_context, buffer, &size, it->second);
    EVP_MD_CTX_cleanup(m_context);

    return std::string(reinterpret_cast<char*>(buffer), size);
}

void signatures_t::verify(const std::string& message, const unsigned char* signature,
                         unsigned int size, const std::string& token)
{
    key_map_t::const_iterator it = m_public_keys.find(token);

    if(it == m_public_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
    
    // Initialize the verification context
    EVP_VerifyInit(m_context, EVP_sha1());

    // Fill it with data
    EVP_VerifyUpdate(m_context, message.data(), message.length());
    
    // Verify the signature
    if(!EVP_VerifyFinal(m_context, signature, size, it->second)) {
        EVP_MD_CTX_cleanup(m_context);
        throw std::runtime_error("invalid signature");
    }

    EVP_MD_CTX_cleanup(m_context);
}
