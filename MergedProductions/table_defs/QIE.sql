-- MySQL dump 10.13  Distrib 5.5.41, for debian-linux-gnu (x86_64)
--
-- Host: e906-db1.fnal.gov    Database: run_012640_R001
-- ------------------------------------------------------
-- Server version	5.1.73-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `QIE`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `QIE` (
  `runID` smallint(5) unsigned NOT NULL,
  `spillID` int(8) unsigned NOT NULL,
  `eventID` int(10) unsigned NOT NULL,
  `rocID` tinyint(3) unsigned NOT NULL,
  `boardID` smallint(5) unsigned NOT NULL,
  `triggerCount` int(11) NOT NULL,
  `turnOnset` int(11) NOT NULL,
  `rfOnset` mediumint(9) NOT NULL,
  `RF-16` mediumint(9) NOT NULL,
  `RF-15` mediumint(9) NOT NULL,
  `RF-14` mediumint(9) NOT NULL,
  `RF-13` mediumint(9) NOT NULL,
  `RF-12` mediumint(9) NOT NULL,
  `RF-11` mediumint(9) NOT NULL,
  `RF-10` mediumint(9) NOT NULL,
  `RF-09` mediumint(9) NOT NULL,
  `RF-08` mediumint(9) NOT NULL,
  `RF-07` mediumint(9) NOT NULL,
  `RF-06` mediumint(9) NOT NULL,
  `RF-05` mediumint(9) NOT NULL,
  `RF-04` mediumint(9) NOT NULL,
  `RF-03` mediumint(9) NOT NULL,
  `RF-02` mediumint(9) NOT NULL,
  `RF-01` mediumint(9) NOT NULL,
  `RF+00` mediumint(9) NOT NULL,
  `RF+01` mediumint(9) NOT NULL,
  `RF+02` mediumint(9) NOT NULL,
  `RF+03` mediumint(9) NOT NULL,
  `RF+04` mediumint(9) NOT NULL,
  `RF+05` mediumint(9) NOT NULL,
  `RF+06` mediumint(9) NOT NULL,
  `RF+07` mediumint(9) NOT NULL,
  `RF+08` mediumint(9) NOT NULL,
  `RF+09` mediumint(9) NOT NULL,
  `RF+10` mediumint(9) NOT NULL,
  `RF+11` mediumint(9) NOT NULL,
  `RF+12` mediumint(9) NOT NULL,
  `RF+13` mediumint(9) NOT NULL,
  `RF+14` mediumint(9) NOT NULL,
  `RF+15` mediumint(9) NOT NULL,
  `RF+16` mediumint(9) NOT NULL,
  KEY `spillID` (`spillID`,`eventID`) USING BTREE,
  KEY `runID` (`runID`,`eventID`) USING BTREE,
  KEY `RF00` (`RF+00`) USING BTREE,
  KEY `rocID` (`rocID`,`boardID`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-01-30  7:39:18
