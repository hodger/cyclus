// Env.h
#if !defined(_ENVIRONMENT)
# define _ENVIRONMENT

#include <string>

#define ENV Env::Instance()

/**
 * A (singleton) environment utility to help locate files and find environment settings 
 */
class Env {
private:
		
	/**
	 * A pointer to this Env once it has been initialized.
	 */
	static Env* instance_;

protected:
		
	/**
	 * Constructs a new Env for this simulation.
	 */
	Env();
	
public:

	/**
	 * Gives all simulation objects global access to the Env by 
	 * returning a pointer to it.
	 *
	 * @return a pointer to the Env
	 */
	static Env* Instance();

  static std::string path_from_cwd_to_cyclus_;

  /**
   * Method to check the existence of and return an environment variable
   *
   * @param var is the variable to check and return 
   */
  std::string checkEnv(std::string var);

};

#endif

